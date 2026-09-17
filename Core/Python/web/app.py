from flask import Flask, jsonify, render_template, request, send_file
import sqlite3
from pathlib import Path

WEB_DIR = Path(__file__).resolve().parent
PYTHON_DIR = WEB_DIR.parent
DB_PATH = PYTHON_DIR / "parking_history.db"
app = Flask(__name__, template_folder=str(WEB_DIR / "templates"), static_folder=str(WEB_DIR / "static"))

def get_db():
    conn = sqlite3.connect(str(DB_PATH), timeout=5)
    conn.row_factory = sqlite3.Row
    return conn

@app.get("/")
def index():
    return render_template("index.html")

@app.get("/api/status")
def api_status():
    conn = get_db()
    try:
        row = conn.execute("SELECT * FROM status_history ORDER BY id DESC LIMIT 1").fetchone()
        if not row:
            return jsonify({"available": False})

        current = conn.execute("""
            SELECT plate, slot, entry_time
            FROM current_parking
            WHERE exit_time IS NULL
            ORDER BY slot
        """).fetchall()

        vehicles = [dict(r) for r in current]
        by_slot = {i: {"occupied": False, "plate": None, "entry_time": None} for i in range(1, 5)}
        for v in vehicles:
            if 1 <= int(v["slot"]) <= 4:
                by_slot[int(v["slot"])] = {
                    "occupied": True,
                    "plate": v["plate"],
                    "entry_time": v["entry_time"],
                }

        occupied_count = len(by_slot) - sum(1 for x in by_slot.values() if not x["occupied"])
        gate = row["gate"]
        gate_hint = "Cổng đang ở trạng thái bình thường"
        if gate == "WAIT_PARK":
            gate_hint = "Đang chờ xe vào vị trí đỗ"
        elif gate == "WAIT_EXIT":
            gate_hint = "Đang chờ xe ra khỏi bãi"
        elif gate == "IDLE":
            gate = "NORMAL"

        return jsonify({
            "available": True,
            "status": dict(row),
            "occupied_count": occupied_count,
            "free_count": 4 - occupied_count,
            "slots": {str(k): v for k, v in by_slot.items()},
            "vehicles": vehicles,
            "gate_hint": gate_hint,
        })
    finally:
        conn.close()

@app.get("/api/events")
def api_events():
    try:
        limit = max(1, min(int(request.args.get("limit", 20)), 200))
    except Exception:
        limit = 20
    conn = get_db()
    try:
        rows = conn.execute("""
            SELECT id, timestamp, event_type, slot, occ, free,
                   in_count, out_count, gate, plate
            FROM parking_events
            ORDER BY id DESC
            LIMIT ?
        """, (limit,)).fetchall()
        return jsonify({"events": [dict(r) for r in rows]})
    finally:
        conn.close()


@app.get("/api/anpr")
def api_anpr():
    """Trạng thái ANPR runtime + biển số mới nhất và hàng đợi chờ ARM KIT."""
    conn = get_db()
    try:
        runtime = conn.execute("SELECT * FROM anpr_runtime WHERE id=1").fetchone()
        pending = conn.execute("""
            SELECT id, timestamp, plate, confidence, processed
            FROM anpr_pending
            ORDER BY id DESC LIMIT 20
        """).fetchall()
        return jsonify({
            "available": runtime is not None,
            "runtime": dict(runtime) if runtime else None,
            "pending": [dict(r) for r in pending],
        })
    except sqlite3.OperationalError as exc:
        return jsonify({"available": False, "error": str(exc), "runtime": None, "pending": []})
    finally:
        conn.close()


@app.get("/api/anpr/frame")
def api_anpr_frame():
    frame_path = PYTHON_DIR / "ANPR" / "data" / "latest_frame.jpg"
    if not frame_path.exists():
        return ("ANPR frame chưa sẵn sàng", 404)
    return send_file(str(frame_path), mimetype="image/jpeg", max_age=0)

if __name__ == "__main__":
    print("Dashboard running at http://127.0.0.1:5000")
    app.run(host="127.0.0.1", port=5000, debug=False)
