# Smart Parking IoT

Hệ thống bãi đỗ xe thông minh sử dụng STM32F207 ARM KIT, Python, SQLite, Web Dashboard, QR và ANPR.

## 1. Tổng quan

Dự án kết hợp phần cứng nhúng với phần mềm PC để theo dõi trạng thái bãi xe, bộ đếm xe vào/ra, dữ liệu lịch sử và giao diện Web Dashboard.

ARM KIT là nguồn quyết định trạng thái parking. Python tiếp nhận dữ liệu qua RS232, xử lý và đồng bộ với SQLite. Web Dashboard đọc dữ liệu từ SQLite để hiển thị.

```text
STM32 / ARM KIT
       |
     RS232
       |
       v
Python Application
       |
       v
SQLite Database
       |
       v
Web Dashboard
```

## 2. Phần cứng

- STM32F207VGTX ARM KIT
- LCD 16x2
- 4 parking slots
- EEPROM 24C16
- RS232
- Nút/cảm biến mô phỏng trạng thái slot và cổng

## 3. Công nghệ

- Embedded C / STM32 HAL
- STM32F207
- STM32CubeIDE
- RS232
- Python 3.11
- SQLite
- Flask
- HTML / CSS / JavaScript
- QR
- ANPR

## 4. Cấu trúc repository

```text
parking_iot/
├── Core/
│   ├── Inc/
│   ├── Src/
│   ├── Startup/
│   └── Python/
│       ├── ANPR/
│       └── web/
├── Drivers/
├── .cproject
├── .mxproject
├── .project
├── parking_iot.ioc
├── setup.bat
├── run_main.bat
├── run_anpr.bat
├── run_web.bat
└── README.md
```

## 5. Yêu cầu

Cần cài:

- Git
- Python 3.11
- STM32CubeIDE
- Driver/USB hoặc giao tiếp phù hợp với ARM KIT

Kiểm tra Python:

```cmd
py -3.11 --version
```

## 6. Cài đặt trên máy mới

Clone repository:

```cmd
git clone https://github.com/NguyenHung952/smart-parking-system.git
```

Vào thư mục:

```cmd
cd smart-parking-system
```

Chạy:

```cmd
setup.bat
```

`setup.bat` sẽ kiểm tra Python 3.11, tạo virtual environment `.venv`, cập nhật pip, cài các package từ các file requirements và kiểm tra ANPR model.

## 7. ANPR model

Model `best.pt` không được lưu trực tiếp trong repository.

Sau khi có model, đặt tại:

```text
Core/Python/ANPR/models/best.pt
```

Xem thêm:

```text
Core/Python/ANPR/models/README.md
```

Repository hiện không cung cấp URL tải model trong README vì nguồn tải model phải được project author xác định riêng.

## 8. STM32

Mở project bằng STM32CubeIDE hoặc import thư mục project hiện có.

Project chính:

```text
parking_iot.ioc
```

Build firmware và nạp vào STM32F207 ARM KIT.

## 9. Chạy Python

```cmd
run_main.bat
```

PC Client xử lý giao tiếp serial, nhận trạng thái ARM KIT và lưu dữ liệu vào SQLite.

## 10. Chạy ANPR

Sau khi đã đặt `best.pt` đúng vị trí:

```cmd
run_anpr.bat
```

ANPR sử dụng camera, detector và OCR để nhận diện biển số.

## 11. Web Dashboard

```cmd
run_web.bat
```

Sau đó mở:

```text
http://127.0.0.1:5000
```

Dashboard hiển thị trạng thái 4 slot, số xe, chỗ trống, bộ đếm IN/OUT, trạng thái cổng, lịch sử sự kiện và thông tin ANPR.

## 12. Luồng dữ liệu

```text
ARM KIT
   ↓
RS232
   ↓
Python Client
   ↓
SQLite
   ↓
Web Dashboard
```

ARM KIT quyết định trạng thái parking. Web Dashboard không tự quyết định IN/OUT.

ANPR chỉ cung cấp thông tin biển số và không tự tạo sự kiện IN/OUT.

## 13. Database

Database runtime được tạo/sử dụng trong quá trình chạy hệ thống.

Các file database runtime không được commit vào repository.

## 14. Các file không nằm trong Git

```text
.venv/
Debug/
build/
dist/
logs/
*.db
*.db-wal
*.db-shm
SmartParking.exe
Core/Python/ANPR/models/best.pt
Core/Python/ANPR/data/latest_frame.jpg
project_tree.txt
```

## 15. Chức năng hiện tại

- [x] STM32/ARM KIT
- [x] 4 parking slots
- [x] LCD 16x2
- [x] EEPROM 24C16
- [x] RS232 communication
- [x] Parking state management
- [x] Vehicle IN/OUT counter
- [x] SQLite database
- [x] Web Dashboard
- [x] QR processing
- [x] ANPR source integration
- [ ] Hoàn thiện toàn bộ quy trình ANPR thực tế
- [ ] Gate motor integration
- [ ] RFID integration

## 16. Tài liệu

Các báo cáo tiến độ và tài liệu hiện trạng được lưu trực tiếp trong repository.

Chi tiết từng phần:

- `Core/Python/README.md` — PC Client và logic xử lý parking
- `Core/Python/ANPR/README_ANPR.md` — ANPR
- `Core/Python/web/README_WEB.md` — Web Dashboard
- `Core/Python/QR_README.md` — QR

## 17. Tác giả

Nguyễn Ngọc Hùng  
Sinh viên Điện tử - Viễn thông, IUH.

## 18. Trạng thái

Smart Parking IoT — đang phát triển.
