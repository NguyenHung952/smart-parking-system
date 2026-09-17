// ============================================================
// SMART PARKING - WEB APP
// ============================================================

async function getJSON(url) {
    const response = await fetch(url, { cache: "no-store" });

    if (!response.ok) {
        throw new Error(await response.text());
    }

    return response.json();
}

function setSlot(number, occupied) {
    const slot = document.getElementById("slot" + number);
    if (!slot) return;

    const isOccupied = Number(occupied) === 1;

    slot.classList.toggle("occupied", isOccupied);
    slot.classList.toggle("free", !isOccupied);

    const state = slot.querySelector(".slot-state");
    if (state) {
        state.textContent = isOccupied ? "CÓ XE" : "TRỐNG";
    }
}

function displayGate(gate) {
    if (!gate || gate === "IDLE") return "NORMAL";
    return gate;
}

function displayGateHint(gate) {
    if (gate === "WAIT_PARK") return "Đang chờ xe vào";
    if (gate === "WAIT_EXIT") return "Đang chờ xe ra";
    return "Trạng thái hệ thống";
}

function displayEvent(eventType) {
    if (eventType === "VEHICLE_IN") return "XE VÀO";
    if (eventType === "VEHICLE_OUT") return "XE RA";
    return eventType || "—";
}

function formatTimestamp(timestamp) {
    if (!timestamp) return "—";

    const match = String(timestamp).match(
        /^(\d{4}-\d{2}-\d{2})T(\d{2}):(\d{2}):(\d{2})/
    );

    if (match) {
        return `${match[1]} ${match[2]}:${match[3]}:${match[4]}`;
    }

    return String(timestamp);
}

async function loadStatus() {
    try {
        const data = await getJSON("/api/status");
        const connection = document.getElementById("connection");

        if (!data.available) {
            if (connection) {
                connection.innerHTML = '<span class="dot"></span>Chưa có dữ liệu';
                connection.classList.add("error");
            }
            return;
        }

        const status = data.status;

        const occ = document.getElementById("occ");
        const free = document.getElementById("free");
        const inCount = document.getElementById("inCount");
        const outCount = document.getElementById("outCount");
        const gate = document.getElementById("gate");
        const gateHint = document.getElementById("gateHint");

        if (occ) occ.textContent = Number(status.occ || 0);
        if (free) free.textContent = Number(status.free || 0);
        if (inCount) inCount.textContent = Number(status.in_count || 0);
        if (outCount) outCount.textContent = Number(status.out_count || 0);
        if (gate) gate.textContent = displayGate(status.gate);
        if (gateHint) gateHint.textContent = displayGateHint(status.gate);

        setSlot(1, status.s1);
        setSlot(2, status.s2);
        setSlot(3, status.s3);
        setSlot(4, status.s4);

        if (connection) {
            connection.innerHTML = '<span class="dot"></span>Đang hoạt động';
            connection.classList.remove("error");
        }

        const updated = document.getElementById("updated");
        if (updated) {
            updated.textContent =
                "Cập nhật: " + new Date().toLocaleTimeString("vi-VN");
        }
    } catch (error) {
        console.error("loadStatus error:", error);

        const connection = document.getElementById("connection");
        if (connection) {
            connection.innerHTML = '<span class="dot"></span>Lỗi kết nối';
            connection.classList.add("error");
        }
    }
}

async function loadEvents() {
    const tbody = document.getElementById("events");
    if (!tbody) return;

    try {
        const data = await getJSON("/api/events?limit=20");

        const events = (data.events || []).filter(event =>
            event.event_type === "VEHICLE_IN" ||
            event.event_type === "VEHICLE_OUT"
        );

        if (events.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="4">Chưa có xe vào / xe ra</td>
                </tr>
            `;
            return;
        }

        tbody.innerHTML = events.map(event => {
            const eventClass =
                event.event_type === "VEHICLE_IN"
                    ? "event-in"
                    : "event-out";

            return `
                <tr>
                    <td>${formatTimestamp(event.timestamp)}</td>
                    <td>
                        <span class="event-badge ${eventClass}">
                            ${displayEvent(event.event_type)}
                        </span>
                    </td>
                    <td>${event.slot ?? "—"}</td>
                    <td>${event.plate ?? "—"}</td>
                </tr>
            `;
        }).join("");
    } catch (error) {
        console.error("loadEvents error:", error);
        tbody.innerHTML = `
            <tr>
                <td colspan="4">Không đọc được dữ liệu</td>
            </tr>
        `;
    }
}

loadStatus();
loadEvents();
setInterval(loadStatus, 2000);
setInterval(loadEvents, 5000);


async function loadANPR() {
    try {
        const data = await getJSON("/api/anpr");
        const runtime = data.runtime || {};
        const status = document.getElementById("anprStatus");
        const plate = document.getElementById("anprPlate");
        const confidence = document.getElementById("anprConfidence");
        const detections = document.getElementById("anprDetections");
        const pending = document.getElementById("anprPending");
        const note = document.getElementById("anprNote");

        if (status) {
            status.textContent = runtime.status || "Chưa chạy";
            status.classList.toggle("error", Number(runtime.camera_ok) !== 1);
        }
        if (plate) plate.textContent = runtime.plate || "—";
        if (confidence) confidence.textContent = runtime.plate ? Number(runtime.confidence || 0).toFixed(2) : "—";
        if (detections) detections.textContent = Number(runtime.detections || 0);
        if (pending) pending.textContent = (data.pending || []).filter(x => Number(x.processed) === 0).length;
        if (note) note.textContent = Number(runtime.camera_ok) === 1
            ? "ANPR đang chạy nền · ARM KIT vẫn là MASTER."
            : "Camera ANPR chưa sẵn sàng: " + (runtime.status || "kiểm tra service");
    } catch (error) {
        console.error("loadANPR error:", error);
    }
}

function refreshANPRFrame() {
    const img = document.getElementById("anprFrame");
    if (!img) return;
    const next = new Image();
    next.onload = () => { img.src = next.src; img.style.opacity = "1"; };
    next.onerror = () => { img.style.opacity = "0.25"; };
    next.src = "/api/anpr/frame?t=" + Date.now();
}

loadANPR();
setInterval(loadANPR, 2000);
refreshANPRFrame();
setInterval(refreshANPRFrame, 500);
