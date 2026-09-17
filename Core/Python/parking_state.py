import re
from dataclasses import dataclass

@dataclass
class ParkingState:
    s1: int = 0
    s2: int = 0
    s3: int = 0
    s4: int = 0
    free: int = 4
    occ: int = 0
    in_count: int = 0
    out_count: int = 0
    gate: str = "IDLE"

    def update_from_status(self, line: str) -> bool:
        """
        Parse:
        STATUS,S1=0,S2=0,S3=0,S4=0,FREE=4,OCC=0,IN=0,OUT=0,GATE=IDLE
        """
        m = re.search(
            r"STATUS,\s*S1=(\d+),\s*S2=(\d+),\s*S3=(\d+),\s*S4=(\d+),"
            r"\s*FREE=(\d+),\s*OCC=(\d+),\s*IN=(\d+),\s*OUT=(\d+),\s*GATE=([A-Za-z0-9_]+)",
            line,
            re.IGNORECASE,
        )
        if not m:
            return False

        self.s1, self.s2, self.s3, self.s4 = map(int, m.group(1, 2, 3, 4))
        self.free, self.occ = map(int, m.group(5, 6))
        self.in_count, self.out_count = map(int, m.group(7, 8))
        self.gate = m.group(9).upper()
        return True

    def as_dict(self):
        return {
            "S1": self.s1, "S2": self.s2, "S3": self.s3, "S4": self.s4,
            "FREE": self.free, "OCC": self.occ,
            "IN": self.in_count, "OUT": self.out_count,
            "GATE": self.gate,
        }

    def display(self):
        slots = [self.s1, self.s2, self.s3, self.s4]
        print("\n========== SMART PARKING ==========")
        print("Slot 1 :", "ĐẦY" if slots[0] else "TRỐNG")
        print("Slot 2 :", "ĐẦY" if slots[1] else "TRỐNG")
        print("Slot 3 :", "ĐẦY" if slots[2] else "TRỐNG")
        print("Slot 4 :", "ĐẦY" if slots[3] else "TRỐNG")
        print("-----------------------------------")
        print(f"Đang có xe : {self.occ}/4")
        print(f"Còn trống  : {self.free}")
        print(f"Tổng vào   : {self.in_count}")
        print(f"Tổng ra     : {self.out_count}")
        print(f"Cổng        : {self.gate}")
        print("===================================\n")
