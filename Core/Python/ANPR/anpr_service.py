"""Smart Parking ANPR background service.

Keeps the tested camera -> YOLO -> OCR -> SQLite logic from test_anpr.py,
but removes the OpenCV GUI. The ARM KIT remains the master: this service
only detects plates and queues them in anpr_pending.
"""
from __future__ import annotations

import sqlite3
import time
from datetime import datetime
from pathlib import Path

import cv2

from camera import ParkingCamera
from detector import LicensePlateDetector
from ocr import PlateOCR

CAMERA_INDEX = 0
CAMERA_WIDTH = 1280
CAMERA_HEIGHT = 720
CAMERA_FPS = 15
MODEL_PATH = "models/best.pt"
YOLO_CONFIDENCE = 0.35
YOLO_IMAGE_SIZE = 640
SAVE_COOLDOWN = 3.0
OCR_INTERVAL = 0.7
FRAME_SAVE_INTERVAL = 0.10
QR_SCAN_COOLDOWN = 2.0
QR_MAX_LENGTH = 64

BASE_DIR = Path(__file__).resolve().parent
PARKING_DB_PATH = BASE_DIR.parent / "parking_history.db"
FRAME_PATH = BASE_DIR / "data" / "latest_frame.jpg"

LATEST_PLATE = ""
LATEST_CONFIDENCE = 0.0
LAST_SAVE_TIME = 0.0
LAST_SAVED_PLATE = None
LAST_OCR_TIME = 0.0
LAST_QR_ID = None
LAST_QR_TIME = 0.0


def normalize_plate(text: str) -> str:
    return str(text or "").upper().strip().replace(" ", "").replace(".", "-")


def is_valid_plate(text: str) -> bool:
    return bool(text) and text.count("-") >= 2


def get_qr_runtime_state():
    try:
        conn = connect()
        row = conn.execute("""
            SELECT direction, plate, status
            FROM qr_runtime WHERE id=1
        """).fetchone()
        conn.close()
        return row or ("", "", "IDLE")
    except Exception:
        return "", "", "IDLE"


def queue_qr(qr_id):
    global LAST_QR_ID, LAST_QR_TIME
    qr_id = str(qr_id or "").strip().upper()[:QR_MAX_LENGTH]
    if not qr_id:
        return False
    now = time.time()
    if qr_id == LAST_QR_ID and now - LAST_QR_TIME < QR_SCAN_COOLDOWN:
        return False
    try:
        conn = connect()
        duplicate = conn.execute("""
            SELECT id FROM qr_pending
            WHERE qr_id=? AND processed=0
              AND julianday(timestamp) >= julianday('now','-5 seconds')
            ORDER BY id DESC LIMIT 1
        """, (qr_id,)).fetchone()
        if duplicate:
            conn.close()
            LAST_QR_ID, LAST_QR_TIME = qr_id, now
            return False
        conn.execute("""
            INSERT INTO qr_pending(timestamp, qr_id, processed)
            VALUES(?,?,0)
        """, (datetime.now().astimezone().isoformat(timespec="seconds"), qr_id))
        conn.commit()
        conn.close()
        LAST_QR_ID, LAST_QR_TIME = qr_id, now
        print(f"[QR] SCANNED | ID={qr_id}")
        return True
    except Exception as exc:
        print(f"[QR] Database error: {exc}")
        return False


def scan_qr_robust(qr_detector, frame):
    """Try several lightweight QR decoding variants in QR-only mode.

    The ANPR pipeline is untouched. This helper is only used after the
    runtime switches to WAIT_QR / PLATE_MISMATCH, when YOLO/OCR are idle.
    Returns: (qr_text, points_in_original_frame).
    """
    if frame is None or frame.size == 0:
        return "", None

    variants = [(frame, 1.0)]

    try:
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        variants.append((gray, 1.0))

        # A small QR can be missed at the native camera resolution.
        # Upscale only in QR-only mode so the ANPR path is unaffected.
        gray_2x = cv2.resize(
            gray, None, fx=2.0, fy=2.0, interpolation=cv2.INTER_CUBIC
        )
        variants.append((gray_2x, 2.0))

        # Adaptive threshold can help when lighting is uneven.
        adaptive = cv2.adaptiveThreshold(
            gray,
            255,
            cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
            cv2.THRESH_BINARY,
            31,
            5,
        )
        variants.append((adaptive, 1.0))

        adaptive_2x = cv2.resize(
            adaptive, None, fx=2.0, fy=2.0, interpolation=cv2.INTER_NEAREST
        )
        variants.append((adaptive_2x, 2.0))
    except Exception:
        # Keep the original color frame available even if preprocessing fails.
        pass

    for image, scale in variants:
        # First try multi-code decoding because it is useful for QR frames
        # where more than one candidate may be detected.
        try:
            ok, decoded_info, points, _ = qr_detector.detectAndDecodeMulti(image)
            if ok and decoded_info:
                for index, text in enumerate(decoded_info):
                    text = str(text or "").strip()
                    if text:
                        if points is not None and len(points) > index:
                            pts = points[index].astype("float32")
                            if scale != 1.0:
                                pts = pts / scale
                            return text, pts
                        return text, None
        except Exception:
            pass

        # Fall back to the single-code decoder for OpenCV versions/frames
        # where detectAndDecodeMulti does not return a result.
        try:
            text, points, _ = qr_detector.detectAndDecode(image)
            text = str(text or "").strip()
            if text:
                if points is not None:
                    pts = points.astype("float32")
                    if scale != 1.0:
                        pts = pts / scale
                    return text, pts
                return text, None
        except Exception:
            pass

    return "", None


def connect():
    conn = sqlite3.connect(str(PARKING_DB_PATH), timeout=10.0)
    conn.execute("PRAGMA busy_timeout = 10000")
    return conn


def init_tables():
    FRAME_PATH.parent.mkdir(parents=True, exist_ok=True)
    conn = connect()
    try:
        conn.execute("PRAGMA journal_mode = WAL")
        conn.execute("""
            CREATE TABLE IF NOT EXISTS anpr_pending (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                plate TEXT NOT NULL,
                confidence REAL NOT NULL DEFAULT 0,
                processed INTEGER NOT NULL DEFAULT 0
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS qr_pending (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                qr_id TEXT NOT NULL,
                processed INTEGER NOT NULL DEFAULT 0
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS qr_runtime (
                id INTEGER PRIMARY KEY CHECK(id=1),
                updated_at TEXT NOT NULL,
                direction TEXT NOT NULL DEFAULT '',
                plate TEXT NOT NULL DEFAULT '',
                qr_id TEXT NOT NULL DEFAULT '',
                status TEXT NOT NULL DEFAULT 'IDLE',
                message TEXT NOT NULL DEFAULT ''
            )
        """)
        conn.execute("""
            INSERT INTO qr_runtime(id,updated_at,status)
            VALUES(1,?, 'IDLE')
            ON CONFLICT(id) DO NOTHING
        """, (datetime.now().astimezone().isoformat(timespec="seconds"),))
        conn.execute("""
            CREATE TABLE IF NOT EXISTS anpr_runtime (
                id INTEGER PRIMARY KEY CHECK (id=1),
                timestamp TEXT NOT NULL,
                plate TEXT NOT NULL DEFAULT '',
                confidence REAL NOT NULL DEFAULT 0,
                detections INTEGER NOT NULL DEFAULT 0,
                camera_ok INTEGER NOT NULL DEFAULT 0,
                status TEXT NOT NULL DEFAULT 'STARTING'
            )
        """)
        conn.execute("""
            INSERT INTO anpr_runtime(id,timestamp,status,camera_ok)
            VALUES(1,?,?,0)
            ON CONFLICT(id) DO NOTHING
        """, (datetime.now().astimezone().isoformat(timespec="seconds"), "STARTING"))
        conn.commit()
    finally:
        conn.close()


def update_runtime(plate="", confidence=0.0, detections=0, camera_ok=1, status="RUNNING"):
    try:
        conn = connect()
        conn.execute("""
            INSERT INTO anpr_runtime(id,timestamp,plate,confidence,detections,camera_ok,status)
            VALUES(1,?,?,?,?,?,?)
            ON CONFLICT(id) DO UPDATE SET
              timestamp=excluded.timestamp, plate=excluded.plate,
              confidence=excluded.confidence, detections=excluded.detections,
              camera_ok=excluded.camera_ok, status=excluded.status
        """, (datetime.now().astimezone().isoformat(timespec="seconds"),
              plate, float(confidence or 0), int(detections), int(camera_ok), status))
        conn.commit()
        conn.close()
    except Exception as exc:
        print(f"[ANPR] runtime DB warning: {exc}")


def queue_plate(plate, confidence=0.0):
    global LAST_SAVE_TIME, LAST_SAVED_PLATE
    plate = normalize_plate(plate)
    if not is_valid_plate(plate):
        return False
    now = time.time()
    if plate == LAST_SAVED_PLATE and now - LAST_SAVE_TIME < SAVE_COOLDOWN:
        return False
    try:
        conn = connect()
        conn.execute("PRAGMA journal_mode = WAL")
        duplicate = conn.execute("""
            SELECT id FROM anpr_pending
            WHERE plate=? AND processed=0
              AND julianday(timestamp) >= julianday('now','-10 seconds')
            ORDER BY id DESC LIMIT 1
        """, (plate,)).fetchone()
        if duplicate:
            conn.close()
            LAST_SAVE_TIME, LAST_SAVED_PLATE = now, plate
            return False
        timestamp = datetime.now().astimezone().isoformat(timespec="seconds")
        conn.execute("INSERT INTO anpr_pending(timestamp,plate,confidence,processed) VALUES(?,?,?,0)",
                     (timestamp, plate, float(confidence or 0)))
        conn.commit()
        conn.close()
        LAST_SAVE_TIME, LAST_SAVED_PLATE = now, plate
        print(f"[ANPR] PLATE QUEUED | Plate={plate} | Confidence={float(confidence or 0):.2f}")
        return True
    except Exception as exc:
        print(f"[ANPR] Database error: {exc}")
        return False


def main():
    global LATEST_PLATE, LATEST_CONFIDENCE, LAST_OCR_TIME
    init_tables()
    camera = ParkingCamera(CAMERA_INDEX, CAMERA_WIDTH, CAMERA_HEIGHT, CAMERA_FPS)
    detector = LicensePlateDetector(MODEL_PATH, YOLO_CONFIDENCE, YOLO_IMAGE_SIZE, "cpu")
    ocr = PlateOCR()
    qr_detector = cv2.QRCodeDetector()
    print("=" * 60)
    print("SMART PARKING - ANPR BACKGROUND SERVICE")
    print("ARM KIT = MASTER | ANPR = CHI NHAN DIEN BIEN SO")
    print("NO OpenCV GUI | NO IN/OUT/SLOT CONTROL")
    print("=" * 60)
    try:
        camera.open()
        update_runtime(camera_ok=1, status="RUNNING")
        last_frame_save = 0.0
        last_qr_poll = 0.0
        qr_direction = ""
        qr_status = "IDLE"
        while True:
            frame = camera.read()
            now = time.time()

            if now - last_qr_poll >= 0.25:
                qr_direction, qr_plate, qr_status = get_qr_runtime_state()
                last_qr_poll = now
            else:
                qr_plate = ""

            # QR workflow is sequential: first ANPR reads the plate; once the
            # main process switches to WAIT_QR, stop YOLO/OCR and scan only QR.
            qr_only = qr_direction in ("IN", "OUT") and qr_status in ("WAIT_QR", "PLATE_MISMATCH")

            if qr_only:
                annotated = frame.copy()
                crops = []
                detections = []
                try:
                    qr_text, qr_points = scan_qr_robust(qr_detector, frame)
                    if qr_text:
                        queue_qr(qr_text)
                    if qr_points is not None:
                        pts = qr_points.astype(int).reshape(-1, 2)
                        for i in range(len(pts)):
                            cv2.line(
                                annotated,
                                tuple(pts[i]),
                                tuple(pts[(i + 1) % len(pts)]),
                                (0, 255, 0),
                                2,
                            )
                except Exception as qr_exc:
                    print(f"[QR] detect warning: {qr_exc}")
            else:
                annotated, crops, detections = detector.detect(frame)
                if crops and now - LAST_OCR_TIME >= OCR_INTERVAL:
                    LAST_OCR_TIME = now
                    for crop in crops:
                        result = ocr.recognize(crop)
                        if result.text:
                            text = normalize_plate(result.text)
                            confidence = float(result.confidence)
                            if is_valid_plate(text):
                                LATEST_PLATE = text
                                LATEST_CONFIDENCE = confidence
                                queue_plate(text, confidence)
                                break
            if now - last_frame_save >= FRAME_SAVE_INTERVAL:
                ok, encoded = cv2.imencode(".jpg", annotated, [int(cv2.IMWRITE_JPEG_QUALITY), 82])
                if ok:
                    FRAME_PATH.write_bytes(encoded.tobytes())
                last_frame_save = now
            update_runtime(LATEST_PLATE, LATEST_CONFIDENCE, len(detections), 1, "RUNNING")
    except Exception as exc:
        print(f"[ANPR] ERROR: {exc}")
        update_runtime(LATEST_PLATE, LATEST_CONFIDENCE, 0, 0, f"ERROR: {exc}")
        raise
    finally:
        camera.release()
        print("[ANPR] Camera released.")


if __name__ == "__main__":
    main()
