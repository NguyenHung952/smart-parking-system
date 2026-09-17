import sqlite3
from datetime import datetime
from pathlib import Path


class PlateHistory:
    """
    Quản lý lịch sử nhận diện biển số cho module ANPR.

    Database riêng:
        ANPR/data/plate_history.db
    """

    def __init__(self, db_path=None):
        if db_path is None:
            base_dir = Path(__file__).resolve().parent
            data_dir = base_dir / "data"
            data_dir.mkdir(parents=True, exist_ok=True)

            self.db_path = data_dir / "plate_history.db"
        else:
            self.db_path = Path(db_path)
            self.db_path.parent.mkdir(parents=True, exist_ok=True)

        self._create_table()

    def _connect(self):
        return sqlite3.connect(str(self.db_path))

    def _create_table(self):
        conn = self._connect()

        try:
            cursor = conn.cursor()

            cursor.execute(
                """
                CREATE TABLE IF NOT EXISTS plate_history (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    plate TEXT NOT NULL,
                    timestamp TEXT NOT NULL,
                    event TEXT NOT NULL,
                    slot INTEGER,
                    created_at TEXT NOT NULL
                )
                """
            )

            conn.commit()

        finally:
            conn.close()

    def add_record(self, plate, event, slot=None, timestamp=None):
        """
        Thêm một bản ghi lịch sử.

        plate:
            Ví dụ: 12-B1-168-88

        event:
            IN hoặc OUT

        slot:
            1, 2, 3 hoặc 4

        timestamp:
            Nếu không truyền sẽ lấy thời gian hiện tại.
        """

        plate = str(plate).strip().upper()
        event = str(event).strip().upper()

        if not plate:
            raise ValueError("Biển số không được rỗng.")

        if event not in ("IN", "OUT"):
            raise ValueError("event chỉ được là IN hoặc OUT.")

        if slot is not None:
            slot = int(slot)

            if slot < 1 or slot > 4:
                raise ValueError("slot phải nằm trong khoảng 1-4.")

        if timestamp is None:
            timestamp = datetime.now().strftime(
                "%d/%m/%Y %H:%M:%S"
            )

        created_at = datetime.now().isoformat(
            timespec="seconds"
        )

        conn = self._connect()

        try:
            cursor = conn.cursor()

            cursor.execute(
                """
                INSERT INTO plate_history
                (
                    plate,
                    timestamp,
                    event,
                    slot,
                    created_at
                )
                VALUES (?, ?, ?, ?, ?)
                """,
                (
                    plate,
                    timestamp,
                    event,
                    slot,
                    created_at
                )
            )

            conn.commit()

            return cursor.lastrowid

        finally:
            conn.close()

    def get_history(self, limit=100):
        """
        Lấy lịch sử mới nhất.

        Trả về list các dictionary.
        """

        conn = self._connect()

        try:
            conn.row_factory = sqlite3.Row
            cursor = conn.cursor()

            cursor.execute(
                """
                SELECT
                    id,
                    plate,
                    timestamp,
                    event,
                    slot,
                    created_at
                FROM plate_history
                ORDER BY id DESC
                LIMIT ?
                """,
                (int(limit),)
            )

            rows = cursor.fetchall()

            return [dict(row) for row in rows]

        finally:
            conn.close()

    def get_by_plate(self, plate, limit=100):
        """
        Lấy lịch sử của một biển số cụ thể.
        """

        plate = str(plate).strip().upper()

        conn = self._connect()

        try:
            conn.row_factory = sqlite3.Row
            cursor = conn.cursor()

            cursor.execute(
                """
                SELECT
                    id,
                    plate,
                    timestamp,
                    event,
                    slot,
                    created_at
                FROM plate_history
                WHERE plate = ?
                ORDER BY id DESC
                LIMIT ?
                """,
                (
                    plate,
                    int(limit)
                )
            )

            rows = cursor.fetchall()

            return [dict(row) for row in rows]

        finally:
            conn.close()

    def clear_history(self):
        """
        Xóa toàn bộ lịch sử ANPR.
        """

        conn = self._connect()

        try:
            cursor = conn.cursor()

            cursor.execute(
                "DELETE FROM plate_history"
            )

            conn.commit()

        finally:
            conn.close()

    def count(self):
        """
        Trả về tổng số bản ghi.
        """

        conn = self._connect()

        try:
            cursor = conn.cursor()

            cursor.execute(
                "SELECT COUNT(*) FROM plate_history"
            )

            return cursor.fetchone()[0]

        finally:
            conn.close()


def get_last_record(self, plate):
    """Trả về bản ghi mới nhất của biển số, hoặc None."""
    conn = sqlite3.connect(self.db_path)
    c = conn.cursor()
    c.execute("SELECT event, slot, timestamp FROM plate_history WHERE plate=? ORDER BY timestamp DESC LIMIT 1", (plate,))
    row = c.fetchone()
    conn.close()
    if row:
        return {"event": row[0], "slot": row[1], "timestamp": row[2]}
    return None

def main():
    """
    Test độc lập plate_history.py
    """

    history = PlateHistory()

    print("[ANPR] Database:")
    print(history.db_path)

    print()
    print("[ANPR] Thêm bản ghi test...")

    history.add_record(
        plate="12-B1-168-88",
        event="IN",
        slot=1
    )

    history.add_record(
        plate="12-B1-168-88",
        event="OUT",
        slot=1
    )

    print("[ANPR] Đã thêm dữ liệu.")
    print()

    print("[ANPR] Lịch sử:")

    records = history.get_history()

    for record in records:
        print(
            f"ID={record['id']} | "
            f"Biển số={record['plate']} | "
            f"Thời gian={record['timestamp']} | "
            f"Sự kiện={record['event']} | "
            f"Vị trí=SLOT {record['slot']}"
        )

    print()
    print(
        f"[ANPR] Tổng số bản ghi: "
        f"{history.count()}"
    )


if __name__ == "__main__":
    main()
