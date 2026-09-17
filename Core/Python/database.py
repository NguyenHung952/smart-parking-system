import sqlite3
from datetime import datetime
from pathlib import Path


class ParkingDatabase:
    """SQLite storage cho Smart Parking.

    ARM KIT la nguon quyet dinh IN/OUT.

    QUY TAC:
      - Chi khi counter IN cua ARM tang va co slot 0 -> 1 thi tao VEHICLE_IN.
      - Chi khi counter OUT cua ARM tang va co slot 1 -> 0 thi tao VEHICLE_OUT.
      - Chi nhan SLOT, khong co gate hop le -> KHONG tao IN/OUT.
      - CỔNG VÀO + chon slot da co xe -> ARM phai giu nguyen slot, Python khong tao IN.
      - CỔNG RA + chon slot trong -> ARM phai giu nguyen slot, Python khong tao OUT.
      - ANPR chi cung cap bien so, khong duoc tu tao IN/OUT.
    """

    PENDING_WINDOW_SECONDS = 15
    # QR authorization binds the car that actually enters after the gate
    # is opened. Keep this separate from the 15-second ANPR queue window.
    QR_ENTRY_AUTH_WINDOW_SECONDS = 300
    PLACEHOLDER_PREFIX = "CHỜ ANPR"

    def __init__(self, db_path="parking_history.db"):
        self.db_path = str(db_path)
        self.conn = None
        self.pending_entry = False
        self.pending_exit = False
        self.pending_entry_time = None
        self.pending_exit_time = None
        self.recent_entry_transitions = []
        self.recent_exit_transitions = []

    def open(self):
        if self.conn is not None:
            return

        Path(self.db_path).parent.mkdir(parents=True, exist_ok=True)
        self.conn = sqlite3.connect(
            self.db_path,
            check_same_thread=False,
            timeout=10.0,
        )
        self.conn.execute("PRAGMA busy_timeout = 10000")
        self.conn.execute("PRAGMA journal_mode = WAL")
        self.conn.execute("PRAGMA synchronous = NORMAL")

        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS status_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT,
                occ INTEGER NOT NULL DEFAULT 0,
                free INTEGER NOT NULL DEFAULT 4,
                in_count INTEGER NOT NULL DEFAULT 0,
                out_count INTEGER NOT NULL DEFAULT 0,
                gate TEXT NOT NULL DEFAULT 'NORMAL',
                s1 INTEGER NOT NULL DEFAULT 0,
                s2 INTEGER NOT NULL DEFAULT 0,
                s3 INTEGER NOT NULL DEFAULT 0,
                s4 INTEGER NOT NULL DEFAULT 0
            )
        """)

        # Database cu co the khong co cot timestamp.
        columns = {
            row[1]
            for row in self.conn.execute("PRAGMA table_info(status_history)")
        }
        if "timestamp" not in columns:
            self.conn.execute(
                "ALTER TABLE status_history ADD COLUMN timestamp TEXT"
            )

        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS parking_events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                event_type TEXT NOT NULL,
                slot INTEGER,
                occ INTEGER NOT NULL,
                free INTEGER NOT NULL,
                in_count INTEGER NOT NULL,
                out_count INTEGER NOT NULL,
                gate TEXT NOT NULL,
                plate TEXT
            )
        """)

        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS current_parking (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                plate TEXT NOT NULL,
                slot INTEGER NOT NULL,
                entry_time TEXT NOT NULL,
                exit_time TEXT
            )
        """)

        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS anpr_pending (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                plate TEXT NOT NULL,
                confidence REAL NOT NULL DEFAULT 0,
                processed INTEGER NOT NULL DEFAULT 0
            )
        """)
        self.conn.commit()

    def close(self):
        if self.conn is not None:
            self.conn.close()
            self.conn = None

    @staticmethod
    def _now_iso():
        return datetime.now().astimezone().isoformat(timespec="seconds")

    @staticmethod
    def _slot_values(state):
        return [state.s1, state.s2, state.s3, state.s4]

    def save_status(self, state, previous_state=None):
        """Luu STATUS va chi xu ly IN/OUT neu ARM da xac nhan bang counter.

        Counter cua ARM la dieu kien bat buoc. Slot transition chi la dieu kien
        thu hai de xac dinh slot nao vua duoc chon.
        """
        self.open()
        now = self._now_iso()

        self.conn.execute("""
            INSERT INTO status_history
            (timestamp, occ, free, in_count, out_count, gate, s1, s2, s3, s4)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            now,
            state.occ,
            state.free,
            state.in_count,
            state.out_count,
            state.gate,
            state.s1,
            state.s2,
            state.s3,
            state.s4,
        ))

        if previous_state is None:
            self.conn.commit()
            return

        old_slots = self._slot_values(previous_state)
        new_slots = self._slot_values(state)

        # ------------------------------------------------------------
        # 1. ARM CỔNG VÀO: counter IN tăng -> mo quyen cho 1 slot 0 -> 1.
        # ------------------------------------------------------------
        if state.in_count > previous_state.in_count:
            self._insert_event(
                now,
                "IN_COUNTER_CHANGED",
                None,
                state,
            )
            self.pending_entry = True
            self.pending_entry_time = now

        # ------------------------------------------------------------
        # 2. ARM CỔNG RA: counter OUT tang -> mo quyen cho 1 slot 1 -> 0.
        # ------------------------------------------------------------
        if state.out_count > previous_state.out_count:
            self._insert_event(
                now,
                "OUT_COUNTER_CHANGED",
                None,
                state,
            )
            self.pending_exit = True
            self.pending_exit_time = now

        # Het han quyen cho phep, tranh mot slot thay doi sau qua lau bi nhan nham.
        self._expire_pending(now)

        # ------------------------------------------------------------
        # 3. Tim DUY NHAT mot slot vua doi.
        # ------------------------------------------------------------
        entry_slots = [
            slot
            for slot, (old, new) in enumerate(
                zip(old_slots, new_slots), start=1
            )
            if old == 0 and new == 1
        ]

        exit_slots = [
            slot
            for slot, (old, new) in enumerate(
                zip(old_slots, new_slots), start=1
            )
            if old == 1 and new == 0
        ]

        # Luu transition hop le ve mat chieu de co the ghep voi counter
        # neu ARM gui hai thong tin o hai STATUS lien tiep.
        if len(entry_slots) == 1:
            self.recent_entry_transitions.append((entry_slots[0], now))
        if len(exit_slots) == 1:
            self.recent_exit_transitions.append((exit_slots[0], now))
        self._expire_transitions(now)

        # Neu dang cho IN ma nhan nham mot slot dang co xe (1 -> 0),
        # huy quyen IN hien tai. Nguoi dung phai bam lai CỔNG VÀO.
        # Tuong tu, dang cho OUT ma nhan nham slot trong (0 -> 1)
        # thi huy quyen OUT.
        if self.pending_entry and exit_slots:
            self.pending_entry = False
            self.pending_entry_time = None
            self.recent_entry_transitions.clear()

        if self.pending_exit and entry_slots:
            self.pending_exit = False
            self.pending_exit_time = None
            self.recent_exit_transitions.clear()

        # ------------------------------------------------------------
        # 4. IN chi khi co quyen IN + dung 0 -> 1.
        # ------------------------------------------------------------
        if self.pending_entry:
            slot = self._take_recent_transition(
                self.recent_entry_transitions, now
            )
            if slot is not None:
                self._handle_arm_entry(slot, state, now)
                self.pending_entry = False
                self.pending_entry_time = None

        # Neu khong co quyen IN thi 0 -> 1 chi la thay doi khong hop le.
        # ARM phai tu chan thay doi nay trong AUTO.

        # ------------------------------------------------------------
        # 5. OUT chi khi co quyen OUT + dung 1 -> 0.
        # ------------------------------------------------------------
        if self.pending_exit:
            slot = self._take_recent_transition(
                self.recent_exit_transitions, now
            )
            if slot is not None:
                self._handle_arm_exit(slot, state, now)
                self.pending_exit = False
                self.pending_exit_time = None

        # 1 -> 0 khong co quyen OUT => KHONG xoa current_parking.

        # ANPR chi bo sung bien so cho event/current parking.
        self._process_pending_anpr(state, now)
        self.conn.commit()

    def _expire_pending(self, now):
        # Dung julianday de phu hop chuoi ISO co timezone.
        for direction in ("entry", "exit"):
            pending = (
                self.pending_entry,
                self.pending_entry_time,
            ) if direction == "entry" else (
                self.pending_exit,
                self.pending_exit_time,
            )

            if not pending[0] or not pending[1]:
                continue

            row = self.conn.execute("""
                SELECT (julianday(?) - julianday(?)) * 86400.0
            """, (now, pending[1])).fetchone()

            age = float(row[0] or 0.0) if row else 0.0
            if age > self.PENDING_WINDOW_SECONDS:
                if direction == "entry":
                    self.pending_entry = False
                    self.pending_entry_time = None
                else:
                    self.pending_exit = False
                    self.pending_exit_time = None

    def _expire_transitions(self, now):
        def keep_recent(items):
            result = []
            for slot, timestamp in items:
                row = self.conn.execute("""
                    SELECT (julianday(?) - julianday(?)) * 86400.0
                """, (now, timestamp)).fetchone()
                age = float(row[0] or 0.0) if row else 0.0
                if 0.0 <= age <= self.PENDING_WINDOW_SECONDS:
                    result.append((slot, timestamp))
            return result[-8:]

        self.recent_entry_transitions = keep_recent(
            self.recent_entry_transitions
        )
        self.recent_exit_transitions = keep_recent(
            self.recent_exit_transitions
        )

    @staticmethod
    def _take_recent_transition(items, now):
        if not items:
            return None
        slot, _timestamp = items[-1]
        items.clear()
        return slot

    def _get_recent_qr_entry_plate(self, timestamp):
        """Return the latest unused QR-IN authorization near this entry."""
        self._ensure_qr_tables()
        row = self.conn.execute("""
            SELECT id, plate
            FROM qr_authorizations
            WHERE direction='IN'
              AND status='AUTHORIZED'
              AND consumed=0
              AND julianday(timestamp) >= julianday(?, ?)
            ORDER BY id DESC
            LIMIT 1
        """, (
            timestamp,
            f'-{int(self.QR_ENTRY_AUTH_WINDOW_SECONDS)} seconds',
        )).fetchone()
        return row

    def _handle_arm_entry(self, slot, state, timestamp):
        # Chi tao event khi ARM da tang counter IN va slot dung 0 -> 1.
        # Neu luot vao duoc mo boi QR, gan bien so da xac thuc truc tiep vao
        # current_parking; khong phu thuoc vao cua so 15 giay cua anpr_pending.
        qr_auth = self._get_recent_qr_entry_plate(timestamp)
        entry_plate = qr_auth[1] if qr_auth else None

        self._insert_event(
            timestamp,
            "VEHICLE_IN",
            slot,
            state,
            plate=entry_plate,
        )

        self._ensure_current_parking_in(
            plate=entry_plate,
            slot=slot,
            timestamp=timestamp,
        )

        if qr_auth:
            self.conn.execute(
                "UPDATE qr_authorizations SET consumed=1 WHERE id=?",
                (qr_auth[0],),
            )

    def _handle_arm_exit(self, slot, state, timestamp):
        # Tim xe dang nam dung slot truoc khi dong record.
        row = self.conn.execute("""
            SELECT id, plate
            FROM current_parking
            WHERE slot=? AND exit_time IS NULL
            ORDER BY id DESC
            LIMIT 1
        """, (slot,)).fetchone()

        plate = row[1] if row else None

        self._insert_event(
            timestamp,
            "VEHICLE_OUT",
            slot,
            state,
            plate=plate,
        )

        # Day la noi DUY NHAT Python dong current_parking do OUT cua ARM.
        self._close_current_parking(
            plate=None,
            slot=slot,
            timestamp=timestamp,
        )

    def _insert_event(self, timestamp, event_type, slot, state, plate=None):
        self.conn.execute("""
            INSERT INTO parking_events
            (timestamp, event_type, slot, occ, free, in_count, out_count, gate, plate)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            timestamp,
            event_type,
            slot,
            state.occ,
            state.free,
            state.in_count,
            state.out_count,
            state.gate,
            plate,
        ))

    def add_anpr_pending(self, plate, confidence=0.0):
        """ANPR chi dua bien so vao hang doi."""
        self.open()
        plate = str(plate).strip().upper()
        if not plate:
            raise ValueError("Biển số không được rỗng.")

        now = self._now_iso()
        cur = self.conn.execute("""
            INSERT INTO anpr_pending
            (timestamp, plate, confidence, processed)
            VALUES (?, ?, ?, 0)
        """, (
            now,
            plate,
            float(confidence or 0.0),
        ))
        self.conn.commit()
        return cur.lastrowid

    def _process_pending_anpr(self, state, now):
        """ANPR khong tao event; chi ghép bien so vao event ARM da xac nhan."""
        pending_rows = self.conn.execute("""
            SELECT id, plate, timestamp
            FROM anpr_pending
            WHERE processed=0
              AND julianday(timestamp) >= julianday('now', '-15 seconds')
            ORDER BY id ASC
        """).fetchall()

        for pending_id, plate, plate_time in pending_rows:
            event_row = self.conn.execute("""
                SELECT id, event_type, slot, timestamp
                FROM parking_events
                WHERE event_type IN ('VEHICLE_IN', 'VEHICLE_OUT')
                  AND plate IS NULL
                  AND slot IS NOT NULL
                  AND julianday(timestamp) >= julianday('now', '-15 seconds')
                ORDER BY
                    ABS((julianday(timestamp) - julianday(?)) * 86400.0),
                    id DESC
                LIMIT 1
            """, (plate_time,)).fetchone()

            if not event_row:
                continue

            event_id, event_type, slot, event_time = event_row
            slot_values = self._slot_values(state)

            if not (1 <= slot <= 4):
                continue

            if event_type == "VEHICLE_IN":
                if slot_values[slot - 1] != 1:
                    continue

                self._set_plate_for_current_slot(
                    slot,
                    plate,
                    event_time,
                )

            elif event_type == "VEHICLE_OUT":
                # OUT da duoc ARM xac nhan. ANPR chi dien bien so vao lich su.
                pass

            self.conn.execute(
                "UPDATE parking_events SET plate=? WHERE id=?",
                (plate, event_id),
            )
            self.conn.execute(
                "UPDATE anpr_pending SET processed=1 WHERE id=?",
                (pending_id,),
            )

    def _ensure_current_parking_in(self, plate, slot, timestamp):
        row = self.conn.execute("""
            SELECT id, plate
            FROM current_parking
            WHERE slot=? AND exit_time IS NULL
            ORDER BY id DESC
            LIMIT 1
        """, (slot,)).fetchone()

        if row:
            if plate and str(row[1]).startswith(self.PLACEHOLDER_PREFIX):
                self.conn.execute(
                    "UPDATE current_parking SET plate=? WHERE id=?",
                    (plate, row[0]),
                )
            return

        self.conn.execute("""
            INSERT INTO current_parking
            (plate, slot, entry_time, exit_time)
            VALUES (?, ?, ?, NULL)
        """, (
            plate or f"{self.PLACEHOLDER_PREFIX} SLOT {slot}",
            slot,
            timestamp,
        ))

    def _set_plate_for_current_slot(self, slot, plate, timestamp):
        row = self.conn.execute("""
            SELECT id, plate
            FROM current_parking
            WHERE slot=? AND exit_time IS NULL
            ORDER BY id DESC
            LIMIT 1
        """, (slot,)).fetchone()

        if row:
            self.conn.execute(
                "UPDATE current_parking SET plate=? WHERE id=?",
                (plate, row[0]),
            )
        else:
            self._ensure_current_parking_in(plate, slot, timestamp)

    def _close_current_parking(self, plate, slot, timestamp):
        if plate:
            self.conn.execute("""
                UPDATE current_parking
                SET exit_time=?
                WHERE plate=? AND slot=? AND exit_time IS NULL
            """, (
                timestamp,
                plate,
                slot,
            ))
        else:
            self.conn.execute("""
                UPDATE current_parking
                SET exit_time=?
                WHERE slot=? AND exit_time IS NULL
            """, (
                timestamp,
                slot,
            ))

    # API cu de tuong thich, nhung khong phai duong xu ly chinh.
    def add_vehicle_in(self, plate, slot):
        self.open()
        now = self._now_iso()
        self._ensure_current_parking_in(plate, slot, now)
        self.conn.commit()

    def remove_vehicle_out(self, plate):
        # Chi giu API cu; khong duoc dung de quyet dinh OUT tu ANPR.
        self.open()
        now = self._now_iso()
        self.conn.execute("""
            UPDATE current_parking
            SET exit_time=?
            WHERE plate=? AND exit_time IS NULL
        """, (now, plate))
        self.conn.commit()


    @staticmethod
    def _canonical_plate(plate):
        return "".join(ch for ch in str(plate or "").upper() if ch.isalnum())

    def _ensure_qr_tables(self):
        self.open()
        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS qr_cards (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                qr_id TEXT UNIQUE NOT NULL,
                plate TEXT NOT NULL,
                owner_name TEXT NOT NULL DEFAULT '',
                active INTEGER NOT NULL DEFAULT 1,
                created_at TEXT NOT NULL
            )
        """)
        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS qr_pending (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                qr_id TEXT NOT NULL,
                processed INTEGER NOT NULL DEFAULT 0
            )
        """)
        self.conn.execute("""
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
        self.conn.execute("""
            INSERT INTO qr_runtime(
                id, updated_at, direction, plate, qr_id, status, message
            ) VALUES(1,?,?,?,?,?,?)
            ON CONFLICT(id) DO NOTHING
        """, (self._now_iso(), '', '', '', 'IDLE', ''))
        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS qr_authorizations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                direction TEXT NOT NULL,
                qr_id TEXT NOT NULL,
                plate TEXT NOT NULL,
                status TEXT NOT NULL,
                consumed INTEGER NOT NULL DEFAULT 0
            )
        """)
        self.conn.commit()

    def set_qr_runtime(
        self,
        direction=None,
        plate=None,
        qr_id=None,
        status=None,
        message=None,
    ):
        self._ensure_qr_tables()
        row = self.conn.execute("""
            SELECT direction, plate, qr_id, status, message
            FROM qr_runtime WHERE id=1
        """).fetchone()
        old = row or ('', '', '', 'IDLE', '')
        values = (
            old[0] if direction is None else str(direction),
            old[1] if plate is None else str(plate),
            old[2] if qr_id is None else str(qr_id),
            old[3] if status is None else str(status),
            old[4] if message is None else str(message),
        )
        self.conn.execute("""
            UPDATE qr_runtime
            SET updated_at=?, direction=?, plate=?, qr_id=?, status=?, message=?
            WHERE id=1
        """, (self._now_iso(), *values))
        self.conn.commit()

    def get_qr_runtime(self):
        self._ensure_qr_tables()
        return self.conn.execute("""
            SELECT updated_at, direction, plate, qr_id, status, message
            FROM qr_runtime WHERE id=1
        """).fetchone()

    def register_qr_card(self, qr_id, plate, owner_name=''):
        self._ensure_qr_tables()
        qr_id = str(qr_id or '').strip().upper()
        plate = str(plate or '').strip().upper()
        owner_name = str(owner_name or '').strip()
        if not qr_id or not plate:
            raise ValueError('QR_ID và biển số không được rỗng.')
        now = self._now_iso()
        self.conn.execute("""
            INSERT INTO qr_cards(qr_id, plate, owner_name, active, created_at)
            VALUES(?,?,?,?,?)
            ON CONFLICT(qr_id) DO UPDATE SET
                plate=excluded.plate,
                owner_name=excluded.owner_name,
                active=1
        """, (qr_id, plate, owner_name, 1, now))
        self.conn.commit()

    def get_qr_cards(self, limit=20):
        self._ensure_qr_tables()
        return self.conn.execute("""
            SELECT qr_id, plate, owner_name, active, created_at
            FROM qr_cards
            ORDER BY id DESC
            LIMIT ?
        """, (int(limit),)).fetchall()

    def latch_latest_plate_for_qr(self, direction, max_age=15):
        self._ensure_qr_tables()
        runtime = self.get_qr_runtime()
        if not runtime or runtime[1] != direction:
            return None
        if runtime[2]:
            return runtime[2]

        row = self.conn.execute("""
            SELECT plate, timestamp
            FROM anpr_pending
            WHERE processed=0
              AND julianday(timestamp) >= julianday('now', ?)
            ORDER BY id DESC
            LIMIT 1
        """, (f'-{int(max_age)} seconds',)).fetchone()
        if not row:
            return None

        plate = str(row[0]).strip().upper()
        self.set_qr_runtime(
            direction=direction,
            plate=plate,
            status='WAIT_QR',
            message='Đã đọc biển số, chờ quét QR',
        )
        return plate

    def queue_qr(self, qr_id):
        self._ensure_qr_tables()
        qr_id = str(qr_id or '').strip().upper()[:64]
        if not qr_id:
            return False
        duplicate = self.conn.execute("""
            SELECT id FROM qr_pending
            WHERE qr_id=? AND processed=0
              AND julianday(timestamp) >= julianday('now','-5 seconds')
            ORDER BY id DESC LIMIT 1
        """, (qr_id,)).fetchone()
        if duplicate:
            return False
        self.conn.execute("""
            INSERT INTO qr_pending(timestamp, qr_id, processed)
            VALUES(?,?,0)
        """, (self._now_iso(), qr_id))
        self.conn.commit()
        return True

    def authorize_latest_qr(self, direction):
        self._ensure_qr_tables()
        runtime = self.get_qr_runtime()
        if not runtime or runtime[1] != direction:
            return None, 'QR_OFF'
        expected_plate = str(runtime[2] or '').strip().upper()
        if not expected_plate:
            return None, 'WAIT_PLATE'

        qr_row = self.conn.execute("""
            SELECT id, qr_id FROM qr_pending
            WHERE processed=0
            ORDER BY id DESC LIMIT 1
        """).fetchone()
        if not qr_row:
            return None, 'WAIT_QR'

        pending_id, qr_id = qr_row
        card = self.conn.execute("""
            SELECT plate FROM qr_cards
            WHERE qr_id=? AND active=1
        """, (qr_id,)).fetchone()

        if not card:
            self.conn.execute(
                "UPDATE qr_pending SET processed=1 WHERE id=?",
                (pending_id,),
            )
            self.conn.commit()
            self.set_qr_runtime(
                qr_id=qr_id,
                status='QR_INVALID',
                message=f'QR {qr_id} chưa đăng ký',
            )
            return None, 'QR_INVALID'

        card_plate = str(card[0]).strip().upper()
        if self._canonical_plate(card_plate) != self._canonical_plate(expected_plate):
            self.conn.execute(
                "UPDATE qr_pending SET processed=1 WHERE id=?",
                (pending_id,),
            )
            self.conn.execute("""
                INSERT INTO qr_authorizations(
                    timestamp,direction,qr_id,plate,status,consumed
                ) VALUES(?,?,?,?,?,0)
            """, (
                self._now_iso(), direction, qr_id,
                expected_plate, 'PLATE_MISMATCH'
            ))
            self.conn.commit()
            self.set_qr_runtime(
                qr_id=qr_id,
                status='PLATE_MISMATCH',
                message=f'QR {qr_id} thuộc {card_plate}, camera đọc {expected_plate}',
            )
            return None, 'PLATE_MISMATCH'

        if direction == 'OUT':
            parked = self.conn.execute("""
                SELECT id, slot, plate
                FROM current_parking
                WHERE exit_time IS NULL
            """).fetchall()
            if not any(
                self._canonical_plate(row[2]) == self._canonical_plate(expected_plate)
                for row in parked
            ):
                self.conn.execute(
                    "UPDATE qr_pending SET processed=1 WHERE id=?",
                    (pending_id,),
                )
                self.conn.commit()
                self.set_qr_runtime(
                    qr_id=qr_id,
                    status='NOT_PARKED',
                    message=f'{expected_plate} không có trong bãi',
                )
                return None, 'NOT_PARKED'

        self.conn.execute("UPDATE qr_pending SET processed=1 WHERE id=?", (pending_id,))
        self.conn.execute("""
            INSERT INTO qr_authorizations(
                timestamp,direction,qr_id,plate,status,consumed
            ) VALUES(?,?,?,?,?,0)
        """, (
            self._now_iso(), direction, qr_id,
            expected_plate, 'AUTHORIZED'
        ))
        self.conn.commit()
        self.set_qr_runtime(
            qr_id=qr_id,
            status='AUTHORIZED',
            message=f'Đã xác thực {direction}',
        )
        return (qr_id, expected_plate), 'AUTHORIZED'

    def get_recent_status(self, limit=20):
        self.open()
        return self.conn.execute("""
            SELECT id, occ, free, in_count, out_count, gate,
                   s1, s2, s3, s4
            FROM status_history
            ORDER BY id DESC
            LIMIT ?
        """, (int(limit),)).fetchall()

    def get_recent_events(self, limit=20):
        self.open()
        return self.conn.execute("""
            SELECT id, timestamp, event_type, slot, occ, free,
                   in_count, out_count, gate, plate
            FROM parking_events
            ORDER BY id DESC
            LIMIT ?
        """, (int(limit),)).fetchall()

    def get_current_parking(self):
        self.open()
        return self.conn.execute("""
            SELECT plate, slot, entry_time
            FROM current_parking
            WHERE exit_time IS NULL
            ORDER BY slot
        """).fetchall()
