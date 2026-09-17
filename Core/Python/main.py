import sys
import argparse
import threading
import time
import shlex
from pathlib import Path

from config import (
    SERIAL_PORT,
    BAUDRATE,
    TIMEOUT,
    STATUS_REQUEST_INTERVAL,
)
from database import ParkingDatabase
from serial_client import SmartParkingSerial

HELP = """
Lệnh PC:
  status        -> hỏi trạng thái STM32
  ping          -> kiểm tra kết nối
  open_in       -> OPEN_GATE_IN
  open_out      -> OPEN_GATE_OUT
  help          -> HELP
  state         -> hiển thị trạng thái cuối cùng
  db_status     -> xem 10 bản ghi STATUS gần nhất
  db_events     -> xem 10 sự kiện vào/ra gần nhất
  qr_in         -> ANPR đọc biển số trước, sau đó quét QR để mở cổng vào
  qr_out        -> ANPR đọc biển số trước, sau đó quét QR để mở cổng ra
  qr_off        -> tắt chế độ QR
  qr_register <QR_ID> <BIEN_SO> [TEN] -> đăng ký QR
  qr_status     -> xem trạng thái QR và thẻ đã đăng ký
  quit          -> thoát
"""

def print_recent_status(db):
    rows = db.get_recent_status(10)
    if not rows:
        print("Database chưa có bản ghi STATUS.")
        return

    print("\n===== 10 STATUS GẦN NHẤT =====")
    for row in rows:
        print(
            f"{row[1]} | S1={row[2]} S2={row[3]} S3={row[4]} S4={row[5]} "
            f"| FREE={row[6]} OCC={row[7]} IN={row[8]} OUT={row[9]} GATE={row[10]}"
        )
    print("===============================\n")

def qr_worker(database, client, stop_event):
    """Sequential QR worker. It never touches YOLO/OCR directly."""
    last_latched = ""
    while not stop_event.wait(0.25):
        try:
            runtime = database.get_qr_runtime()
            if not runtime:
                continue
            _updated, direction, plate, qr_id, status, message = runtime

            if direction not in ("IN", "OUT"):
                last_latched = ""
                continue

            if status in ("WAIT_PLATE", "READY") and not plate:
                latched = database.latch_latest_plate_for_qr(direction)
                if latched and latched != last_latched:
                    last_latched = latched
                    print(f"[QR] BIEN SO DA LUU | {direction} | {latched}")
                    print("[QR] Bây giờ chỉ cần đưa QR CARD vào camera...")
                continue

            if status in ("WAIT_QR", "PLATE_MISMATCH"):
                result, auth_status = database.authorize_latest_qr(direction)
                if result and auth_status == "AUTHORIZED":
                    auth_qr, auth_plate = result
                    command = "OPEN_GATE_IN" if direction == "IN" else "OPEN_GATE_OUT"
                    client.send(command)
                    print(
                        f"[QR] AUTHORIZED | {direction} | {auth_qr} | "
                        f"{auth_plate} | TX={command}"
                    )
                    database.set_qr_runtime(
                        direction="", plate=auth_plate, qr_id=auth_qr,
                        status="AUTHORIZED", message=f"Đã gửi {command}"
                    )
                    last_latched = ""
                elif auth_status not in ("WAIT_QR", "WAIT_PLATE"):
                    print(f"[QR] {auth_status}")
                    # Allow user to present another QR after mismatch/invalid.
                    if direction in ("IN", "OUT"):
                        database.set_qr_runtime(status="WAIT_QR")
        except Exception as exc:
            print(f"[QR WORKER ERROR] {exc}")


def print_qr_status(db):
    runtime = db.get_qr_runtime()
    if runtime:
        print(
            f"QR runtime: direction={runtime[1] or '-'} | "
            f"plate={runtime[2] or '-'} | qr={runtime[3] or '-'} | "
            f"status={runtime[4]} | {runtime[5]}"
        )
    rows = db.get_qr_cards(10)
    if rows:
        print("===== 10 QR GẦN NHẤT =====")
        for row in rows:
            print(f"{row[0]} -> {row[1]} | {row[2] or '-'} | active={row[3]}")
    else:
        print("Chưa có QR nào được đăng ký.")


def print_recent_events(db):
    rows = db.get_recent_events(10)
    if not rows:
        print("Database chưa có sự kiện vào/ra.")
        return

    print("\n===== 10 SỰ KIỆN GẦN NHẤT =====")
    for row in rows:
        slot = f"Slot {row[3]}" if row[3] is not None else "Slot ?"
        print(
            f"{row[1]} | {row[2]} | {slot} | "
            f"OCC={row[4]} FREE={row[5]} IN={row[6]} OUT={row[7]} GATE={row[8]}"
        )
    print("================================\n")

def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--background", action="store_true")
    args = parser.parse_args()
    # Luôn dùng database nằm cạnh main.py, không phụ thuộc vào thư mục CMD hiện tại.
    # web/app.py cũng đọc chính file này: Core/Python/parking_history.db.
    database_path = Path(__file__).resolve().parent / "parking_history.db"
    database = ParkingDatabase(str(database_path))
    database.open()

    client = SmartParkingSerial(
        SERIAL_PORT,
        BAUDRATE,
        TIMEOUT,
        database=database,
    )

    try:
        print(f"Mở {SERIAL_PORT} @ {BAUDRATE},8N1 ...")
        client.open()
        print("PC Client đã kết nối STM32.")
        print("SQLite Database: parking_history.db")
        print(HELP)

        # Hỏi trạng thái một lần khi kết nối.
        client.get_status()

        # Nếu được bật trong config.py, tự hỏi STM32 định kỳ để
        # Database/Web nhận trạng thái cảm biến mới mà không cần nhập "status".
        stop_status_polling = threading.Event()

        def status_polling_loop():
            while not stop_status_polling.wait(STATUS_REQUEST_INTERVAL):
                if not client.running:
                    break
                try:
                    client.get_status()
                except Exception as exc:
                    print(f"[STATUS POLLING ERROR] {exc}")

        polling_thread = threading.Thread(
            target=status_polling_loop,
            daemon=True,
        )
        polling_thread.start()

        stop_qr_worker = threading.Event()
        qr_thread = threading.Thread(
            target=qr_worker,
            args=(database, client, stop_qr_worker),
            daemon=True,
        )
        qr_thread.start()

        if args.background:
            print("[MAIN] Background mode: không chờ lệnh console.")
            while client.running:
                time.sleep(1.0)
        else:
            while client.running:
                try:
                    raw_cmd = input("PC> ").strip()
                except (EOFError, KeyboardInterrupt):
                    break

                if not raw_cmd:
                    continue
                try:
                    parts = shlex.split(raw_cmd)
                except ValueError as exc:
                    print(f"Lệnh không hợp lệ: {exc}")
                    continue
                cmd = parts[0].lower()

                if cmd == "status":
                    client.send("GET_STATUS")
                elif cmd == "ping":
                    client.send("PING")
                elif cmd == "open_in":
                    client.send("OPEN_GATE_IN")
                elif cmd == "open_out":
                    client.send("OPEN_GATE_OUT")
                elif cmd == "help":
                    client.send("HELP")
                elif cmd == "state":
                    client.state.display()
                elif cmd == "db_status":
                    print_recent_status(database)
                elif cmd == "db_events":
                    print_recent_events(database)
                elif cmd == "qr_in":
                    database.set_qr_runtime(
                        direction="IN", plate="", qr_id="", status="WAIT_PLATE",
                        message="Đọc biển số trước, sau đó quét QR"
                    )
                    print("[QR] CỔNG VÀO: chờ ANPR đọc biển số trước...")
                elif cmd == "qr_out":
                    database.set_qr_runtime(
                        direction="OUT", plate="", qr_id="", status="WAIT_PLATE",
                        message="Đọc biển số trước, sau đó quét QR"
                    )
                    print("[QR] CỔNG RA: chờ ANPR đọc biển số trước...")
                elif cmd == "qr_off":
                    database.set_qr_runtime(
                        direction="", plate="", qr_id="", status="IDLE",
                        message="Tắt QR"
                    )
                    print("[QR] Đã tắt chế độ QR.")
                elif cmd == "qr_register":
                    if len(parts) < 3:
                        print("Dùng: qr_register <QR_ID> <BIEN_SO> [TEN]")
                    else:
                        qr_id = parts[1].upper()
                        plate = parts[2].upper()
                        owner = " ".join(parts[3:]) if len(parts) > 3 else ""
                        database.register_qr_card(qr_id, plate, owner)
                        print(f"[QR] Đã đăng ký {qr_id} -> {plate}")
                elif cmd == "qr_status":
                    print_qr_status(database)
                elif cmd in ("quit", "exit", "q"):
                    break
                else:
                    print("Lệnh không hợp lệ." + HELP)

    except Exception as exc:
        print(f"\nKhông thể kết nối: {exc}")
        print("Kiểm tra COM, cáp USB-RS232 và Hercules có đang mở COM5 hay không.")
        return 1
    finally:
        if 'stop_status_polling' in locals():
            stop_status_polling.set()
        if 'stop_qr_worker' in locals():
            stop_qr_worker.set()
        client.close()
        database.close()
        print("Đã đóng kết nối.")

    return 0

if __name__ == "__main__":
    sys.exit(main())
