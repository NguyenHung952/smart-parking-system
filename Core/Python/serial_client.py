import threading
import serial

from parking_state import ParkingState
from database import ParkingDatabase

class SmartParkingSerial:
    def __init__(
        self,
        port: str,
        baudrate: int,
        timeout: float = 0.2,
        database: ParkingDatabase = None,
    ):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None
        self.running = False
        self.state = ParkingState()
        self.rx_thread = None
        self.database = database
        self._has_previous_state = False

    def open(self):
        self.ser = serial.Serial(
            port=self.port,
            baudrate=self.baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=self.timeout,
            write_timeout=1.0,
        )
        self.running = True
        self.rx_thread = threading.Thread(target=self._receive_loop, daemon=True)
        self.rx_thread.start()

    def close(self):
        self.running = False
        if self.ser and self.ser.is_open:
            self.ser.close()

    def send(self, command: str):
        if not self.ser or not self.ser.is_open:
            raise RuntimeError("Serial port chưa mở.")
        command = command.strip()
        if not command:
            return
        self.ser.write((command + "\r\n").encode("ascii"))
        self.ser.flush()

    def get_status(self):
        self.send("GET_STATUS")

    def _receive_loop(self):
        while self.running:
            try:
                raw = self.ser.readline()
                if not raw:
                    continue

                text = raw.decode("ascii", errors="replace").strip()
                if not text:
                    continue

                print(f"[RX] {text}")

                # Hercules/thiết bị trung gian có thể làm echo hoặc nối chuỗi.
                # Chỉ lấy phần STATUS hợp lệ để parser xử lý.
                if "STATUS," in text.upper():
                    idx = text.upper().find("STATUS,")
                    status = text[idx:]

                    # Giữ bản sao trạng thái trước khi cập nhật.
                    previous_state = None
                    if self._has_previous_state:
                        previous_state = ParkingState(
                            self.state.s1, self.state.s2,
                            self.state.s3, self.state.s4,
                            self.state.free, self.state.occ,
                            self.state.in_count, self.state.out_count,
                            self.state.gate
                        )

                    if self.state.update_from_status(status):
                        if self.database is not None:
                            try:
                                self.database.save_status(
                                    self.state, previous_state
                                )
                            except Exception as exc:
                                # Database lỗi không được làm dừng giao tiếp RS232.
                                print(f"[DATABASE ERROR] {exc}")

                        self._has_previous_state = True
                        self.state.display()

            except serial.SerialException as exc:
                print(f"[SERIAL ERROR] {exc}")
                break
            except Exception as exc:
                print(f"[RX ERROR] {exc}")

        self.running = False
