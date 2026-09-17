# Smart Parking - PC RS232 Client + SQLite

## Luật xử lý hiện tại

ARM KIT là nguồn quyết định trạng thái và luồng xe.

### Xe vào

```text
Nhấn CỔNG VÀO
      ↓
WAIT_PARK
      ↓
Chọn một SLOT đang TRỐNG
      ↓
SLOT 0 → 1
      ↓
IN +1
```

Nếu chọn SLOT đã có xe khi đang chờ CỔNG VÀO: **SLOT không được đổi và không tăng IN**.

### Xe ra

```text
Nhấn CỔNG RA
      ↓
WAIT_EXIT
      ↓
Chọn đúng SLOT đang CÓ XE
      ↓
SLOT 1 → 0
      ↓
OUT +1
      ↓
Xe ở SLOT đó được đóng khỏi current_parking
```

Nếu chỉ nhấn SLOT, hoặc nhấn CỔNG RA rồi chọn SLOT đang trống: **không tính OUT và không xóa xe**.

ANPR chỉ nhận diện và cung cấp biển số; ANPR không tự tạo IN/OUT.

## Kết nối

PC/Laptop -> USB-RS232 -> RS232 ARM KIT -> USART1 STM32

Thông số mặc định:
- COM5 (có thể đổi trong `config.py`)
- 115200 baud
- 8 data bits
- No parity
- 1 stop bit

## Chức năng

PC Client:
- Mở cổng serial.
- Gửi `PING`.
- Gửi `GET_STATUS`.
- Nhận và phân tích gói `STATUS`.
- Hiển thị trạng thái 4 slot.
- Hiển thị FREE/OCC/IN/OUT/GATE.
- Gửi `OPEN_GATE_IN` và `OPEN_GATE_OUT`.
- Lưu STATUS vào SQLite.
- Chỉ tạo `VEHICLE_IN` / `VEHICLE_OUT` khi ARM đã xác nhận đúng điều kiện cổng + slot.

## Database

Database nằm tại:

```text
Core/Python/parking_history.db
```

Các bảng chính:
- `status_history`: trạng thái ARM gửi về.
- `parking_events`: lịch sử xe vào/ra.
- `current_parking`: xe đang được ghi nhận trong bãi.
- `anpr_pending`: hàng đợi biển số từ ANPR.

## Chạy

Đóng Hercules trước nếu Hercules đang chiếm COM5.

```bash
python main.py
```

Web Dashboard:

```bash
python web/app.py
```

Mở trình duyệt tại:

```text
http://127.0.0.1:5000
```
