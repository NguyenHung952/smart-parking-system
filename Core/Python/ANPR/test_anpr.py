import cv2
import time
import sqlite3
from pathlib import Path
from datetime import datetime

from camera import ParkingCamera
from detector import LicensePlateDetector
from ocr import PlateOCR

# ============================================================
# SMART PARKING - ANPR
# ARM KIT LÀ MASTER - ANPR CHỈ NHẬN DIỆN BIỂN SỐ
# ============================================================

CAMERA_INDEX = 0
CAMERA_WIDTH = 1280
CAMERA_HEIGHT = 720
CAMERA_FPS = 15
MODEL_PATH = "models/best.pt"
YOLO_CONFIDENCE = 0.35
YOLO_IMAGE_SIZE = 640

SAVE_COOLDOWN = 3.0
OCR_INTERVAL = 0.7
PENDING_WINDOW_SECONDS = 15

WINDOW_NAME = "Smart Parking - ANPR"
CONTROL_HEIGHT = 130

TEXT_COLOR = (255, 255, 255)
PANEL_COLOR = (45, 45, 45)
OK_COLOR = (0, 200, 0)
INFO_COLOR = (0, 200, 255)

LATEST_PLATE = ""
LATEST_CONFIDENCE = 0.0
STATUS_TEXT = "READY - ANPR chỉ nhận diện biển số."
LAST_SAVE_TIME = 0.0
LAST_SAVED_PLATE = None
LAST_OCR_TIME = 0.0

# Cùng database với main.py, nhưng ANPR chỉ ghi vào bảng anpr_pending.
PARKING_DB_PATH = Path(__file__).resolve().parent.parent / "parking_history.db"


def normalize_plate(text):
    if not text:
        return ""
    return str(text).upper().strip().replace(" ", "").replace(".", "-")


def is_valid_plate(text):
    return bool(text) and text.count("-") >= 2


def init_pending_table():
    """Chỉ tạo bảng chờ cho ANPR. Không tạo/sửa trạng thái parking."""
    try:
        conn = sqlite3.connect(str(PARKING_DB_PATH), timeout=10.0)
        conn.execute("PRAGMA busy_timeout = 10000")
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
        conn.commit()
        conn.close()
    except Exception as exc:
        print(f"[ANPR] Không thể khởi tạo ANPR pending: {exc}")


def queue_plate(plate, confidence=0.0):
    """Đưa BIỂN SỐ vào hàng đợi. Không có IN/OUT/SLOT ở đây."""
    global LAST_SAVE_TIME, LAST_SAVED_PLATE, STATUS_TEXT

    plate = normalize_plate(plate)
    if not is_valid_plate(plate):
        STATUS_TEXT = f"BIỂN KHÔNG HỢP LỆ: {plate or '---'}"
        return False

    now_epoch = time.time()
    if (
        plate == LAST_SAVED_PLATE
        and now_epoch - LAST_SAVE_TIME < SAVE_COOLDOWN
    ):
        return False

    try:
        conn = sqlite3.connect(str(PARKING_DB_PATH), timeout=10.0)
        conn.execute("PRAGMA busy_timeout = 10000")
        conn.execute("PRAGMA journal_mode = WAL")

        # Không tạo hàng đợi trùng liên tục trong cùng một khoảng thời gian.
        duplicate = conn.execute("""
            SELECT id
            FROM anpr_pending
            WHERE plate=?
              AND processed=0
              AND julianday(timestamp) >= julianday('now', '-10 seconds')
            ORDER BY id DESC
            LIMIT 1
        """, (plate,)).fetchone()

        if duplicate:
            conn.close()
            LAST_SAVE_TIME = now_epoch
            LAST_SAVED_PLATE = plate
            return False

        timestamp = datetime.now().astimezone().isoformat(timespec="seconds")
        conn.execute("""
            INSERT INTO anpr_pending
            (timestamp, plate, confidence, processed)
            VALUES (?, ?, ?, 0)
        """, (timestamp, plate, float(confidence or 0.0)))
        conn.commit()
        conn.close()

        LAST_SAVE_TIME = now_epoch
        LAST_SAVED_PLATE = plate
        STATUS_TEXT = f"ĐÃ NHẬN BIỂN SỐ: {plate} → CHỜ ARM KIT"
        print(
            f"[ANPR] PLATE QUEUED | Plate={plate} | "
            f"Confidence={float(confidence or 0.0):.2f}"
        )
        return True

    except sqlite3.OperationalError as exc:
        STATUS_TEXT = f"LỖI DB: {exc}"
        print(f"[ANPR] Database error: {exc}")
        return False
    except Exception as exc:
        STATUS_TEXT = f"LỖI: {exc}"
        print(f"[ANPR] Error: {exc}")
        return False


def get_buttons(width, height):
    """Chỉ còn nút CLEAR/EXIT. Không có nút IN/OUT/SLOT."""
    y0 = height - CONTROL_HEIGHT + 45
    button_h = 45
    gap = 10
    margin = 20
    button_w = 180

    return {
        "CLEAR": (margin, y0, margin + button_w, y0 + button_h),
        "EXIT": (
            margin + button_w + gap,
            y0,
            margin + button_w * 2 + gap,
            y0 + button_h,
        ),
    }


def point_in_rect(x, y, rect):
    x1, y1, x2, y2 = rect
    return x1 <= x <= x2 and y1 <= y <= y2


def mouse_callback(event, x, y, flags, param):
    global LATEST_PLATE, LATEST_CONFIDENCE, STATUS_TEXT

    if event != cv2.EVENT_LBUTTONDOWN:
        return

    buttons = get_buttons(CAMERA_WIDTH, CAMERA_HEIGHT + CONTROL_HEIGHT)

    if point_in_rect(x, y, buttons["CLEAR"]):
        LATEST_PLATE = ""
        LATEST_CONFIDENCE = 0.0
        STATUS_TEXT = "Đã CLEAR. ANPR tiếp tục nhận diện."
        return

    if point_in_rect(x, y, buttons["EXIT"]):
        STATUS_TEXT = "Đang thoát..."
        param["exit"] = True


def draw_controls(canvas):
    height, width = canvas.shape[:2]
    panel_y = height - CONTROL_HEIGHT
    cv2.rectangle(canvas, (0, panel_y), (width, height), PANEL_COLOR, -1)

    cv2.putText(
        canvas,
        "ARM KIT = MASTER | ANPR = CHI NHAN DIEN BIEN SO",
        (20, panel_y + 27),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.65,
        INFO_COLOR,
        2,
    )

    cv2.putText(
        canvas,
        STATUS_TEXT[:120],
        (20, panel_y + 55),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.58,
        OK_COLOR if "ĐÃ NHẬN" in STATUS_TEXT else INFO_COLOR,
        2,
    )

    buttons = get_buttons(width, height)
    for name, rect in buttons.items():
        x1, y1, x2, y2 = rect
        cv2.rectangle(canvas, (x1, y1), (x2, y2), (70, 70, 70), -1)
        cv2.rectangle(canvas, (x1, y1), (x2, y2), (180, 180, 180), 1)
        text_size = cv2.getTextSize(
            name, cv2.FONT_HERSHEY_SIMPLEX, 0.65, 2
        )[0]
        tx = x1 + (x2 - x1 - text_size[0]) // 2
        ty = y1 + (y2 - y1 + text_size[1]) // 2
        cv2.putText(
            canvas,
            name,
            (tx, ty),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.65,
            TEXT_COLOR,
            2,
        )


def main():
    global LATEST_PLATE, LATEST_CONFIDENCE, STATUS_TEXT, LAST_OCR_TIME

    init_pending_table()

    camera = ParkingCamera(
        CAMERA_INDEX, CAMERA_WIDTH, CAMERA_HEIGHT, CAMERA_FPS
    )
    detector = LicensePlateDetector(
        MODEL_PATH, YOLO_CONFIDENCE, YOLO_IMAGE_SIZE, "cpu"
    )
    ocr = PlateOCR()

    print("=" * 60)
    print("SMART PARKING - ANPR")
    print("ARM KIT = MASTER | ANPR = CHI NHAN DIEN BIEN SO")
    print("ANPR KHONG GUI IN / OUT / SLOT")
    print("=" * 60)

    try:
        camera.open()
        print("[OK] Camera opened.")
        cv2.namedWindow(WINDOW_NAME, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(
            WINDOW_NAME,
            CAMERA_WIDTH,
            CAMERA_HEIGHT + CONTROL_HEIGHT,
        )

        mouse_state = {"exit": False}
        cv2.setMouseCallback(WINDOW_NAME, mouse_callback, mouse_state)

        while True:
            frame = camera.read()
            annotated, crops, detections = detector.detect(frame)

            now = time.time()
            if crops and now - LAST_OCR_TIME >= OCR_INTERVAL:
                LAST_OCR_TIME = now
                current_plate = ""
                current_confidence = 0.0

                for i, crop in enumerate(crops):
                    result = ocr.recognize(crop)
                    if result.text:
                        text = normalize_plate(result.text)
                        confidence = float(result.confidence)
                        print(
                            f"[OCR] Plate {i}: {text} "
                            f"(conf={confidence:.2f})"
                        )
                        cv2.putText(
                            annotated,
                            text,
                            (20, 70 + i * 35),
                            cv2.FONT_HERSHEY_SIMPLEX,
                            0.9,
                            OK_COLOR,
                            2,
                        )

                        if not current_plate and is_valid_plate(text):
                            current_plate = text
                            current_confidence = confidence

                if current_plate:
                    LATEST_PLATE = current_plate
                    LATEST_CONFIDENCE = current_confidence
                    queue_plate(current_plate, current_confidence)

            if LATEST_PLATE:
                cv2.putText(
                    annotated,
                    f"Plate: {LATEST_PLATE}",
                    (20, 70),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.9,
                    OK_COLOR,
                    2,
                )

            cv2.putText(
                annotated,
                f"License plates: {len(detections)}",
                (20, 35),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.8,
                OK_COLOR,
                2,
            )
            cv2.putText(
                annotated,
                "ARM KIT CONTROLS PARKING - ANPR ONLY READS PLATE",
                (20, CAMERA_HEIGHT - 20),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.60,
                TEXT_COLOR,
                2,
            )

            canvas = cv2.copyMakeBorder(
                annotated,
                0,
                CONTROL_HEIGHT,
                0,
                0,
                cv2.BORDER_CONSTANT,
                value=PANEL_COLOR,
            )
            draw_controls(canvas)
            cv2.imshow(WINDOW_NAME, canvas)

            key = cv2.waitKey(1) & 0xFF
            if key in (ord("q"), ord("Q"), 27):
                break
            if key == ord("c") or key == ord("C"):
                LATEST_PLATE = ""
                LATEST_CONFIDENCE = 0.0
                STATUS_TEXT = "Đã CLEAR."

            if mouse_state["exit"]:
                break

    except Exception as exc:
        print("\n[ANPR] ERROR:", exc)
        STATUS_TEXT = f"LỖI: {exc}"
    finally:
        camera.release()
        cv2.destroyAllWindows()
        print("\n[ANPR] Camera released.")
        print("[ANPR] Test finished.")


if __name__ == "__main__":
    main()
