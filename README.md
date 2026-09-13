# 🚗 Smart Parking IoT

Hệ thống **bãi đỗ xe thông minh** xây dựng trên nền tảng **STM32/ARM Kit**, kết hợp cảm biến, LCD 16x2, EEPROM, giao tiếp RS232, phần mềm Python, cơ sở dữ liệu SQLite và Web Dashboard.

Dự án hướng đến việc mô phỏng một hệ thống quản lý bãi xe có khả năng **nhận trạng thái chỗ đỗ, quản lý số lượng xe, ghi nhận sự kiện và hiển thị dữ liệu theo thời gian thực trên máy tính**. Hệ thống cũng có định hướng tích hợp **nhận diện biển số xe (ANPR)**.

> Dự án được thực hiện trên ARM Kit do **Công ty Điện Tự Động Phú Cường** cung cấp.

---

## 🎯 Mục tiêu dự án

- Theo dõi trạng thái các vị trí đỗ xe.
- Quản lý số xe vào/ra và số chỗ còn trống.
- Hiển thị trạng thái trên **LCD 16x2**.
- Lưu trữ dữ liệu và lịch sử hoạt động.
- Truyền dữ liệu giữa Kit và máy tính qua **RS232**.
- Xây dựng **Web Dashboard** để giám sát bãi xe.
- Tạo nền tảng để tích hợp **ANPR – Automatic Number Plate Recognition**.

---

## 🏗️ Kiến trúc hệ thống

```text
┌──────────────────────────────┐
│       STM32 / ARM Kit        │
│                              │
│  Sensors • LCD 16x2 • EEPROM │
└──────────────┬───────────────┘
               │
             RS232
               │
               ▼
┌──────────────────────────────┐
│        Python Application    │
│                              │
│  Serial Communication        │
│  Parking State Processing    │
│  Event / Counter Handling    │
└──────────────┬───────────────┘
               │
               ▼
┌──────────────────────────────┐
│        SQLite Database       │
│                              │
│  Parking State / Events      │
│  Status History              │
└──────────────┬───────────────┘
               │
               ▼
┌──────────────────────────────┐
│        Web Dashboard         │
│                              │
│  Parking Status              │
│  Vehicle Counters            │
│  Available Slots             │
│  Event History               │
└──────────────────────────────┘
```

### Luồng dữ liệu

```text
Sensor / STM32
      ↓
   RS232
      ↓
 Python PC
      ↓
 SQLite Database
      ↓
 Web Dashboard
```

Kit là nguồn dữ liệu trạng thái của hệ thống. Python đóng vai trò trung gian tiếp nhận, xử lý và đồng bộ dữ liệu với cơ sở dữ liệu và Web Dashboard.

---

## 🔧 Thành phần chính

| Thành phần | Vai trò |
|---|---|
| **STM32 / ARM Kit** | Điều khiển và xử lý trạng thái phần cứng |
| **Cảm biến** | Phát hiện trạng thái vị trí đỗ / sự kiện xe |
| **LCD 16x2** | Hiển thị thông tin trực tiếp trên Kit |
| **EEPROM** | Lưu trữ dữ liệu cần duy trì trên thiết bị |
| **RS232** | Giao tiếp giữa Kit và máy tính |
| **Python** | Nhận dữ liệu, xử lý trạng thái và quản lý hệ thống |
| **SQLite** | Lưu trạng thái, bộ đếm và lịch sử sự kiện |
| **Flask Web Dashboard** | Hiển thị và giám sát hệ thống trên trình duyệt |
| **ANPR** | Nhận diện biển số xe, dùng cho hướng mở rộng |

---

## 📊 Web Dashboard

Web Dashboard cung cấp các thông tin chính của bãi xe:

- **Tổng xe vào**
- **Đang có xe**
- **Còn trống**
- **Trạng thái từng slot**
- **Trạng thái cổng**
- **Lịch sử sự kiện**

Dashboard được thiết kế để dữ liệu trên web được đồng bộ với trạng thái thực tế nhận từ Kit.

### Một số trạng thái sự kiện

| Sự kiện hệ thống | Hiển thị trên Web |
|---|---|
| `VEHICLE_IN` | `XE ĐANG VÀO` |
| `VEHICLE_OUT` | `XE ĐANG RA` |
| `IN_COUNTER_CHANGED` | `CỔNG VÀO MỞ` |
| `OUT_COUNTER_CHANGED` | `CỔNG RA MỞ` |

**Lưu ý:** sự kiện `CỔNG RA MỞ` chỉ phản ánh việc cổng ra mở, **không tự động cộng thêm một xe vào số xe đang có trong bãi hoặc tính thêm slot xe**.

---

## 💾 Dữ liệu và đồng bộ

Hệ thống sử dụng SQLite để lưu dữ liệu phục vụ Web Dashboard, bao gồm trạng thái bãi xe và lịch sử sự kiện.

Cơ chế hoạt động tổng quát:

1. Kit gửi trạng thái qua RS232.
2. Python nhận và xử lý dữ liệu.
3. Trạng thái bãi xe được cập nhật vào hệ thống dữ liệu.
4. Web Dashboard đọc dữ liệu và hiển thị cho người dùng.
5. Lịch sử sự kiện được lưu để theo dõi hoạt động của bãi xe.

---

## 🧪 Các chức năng chính

- [x] Giao tiếp STM32/ARM Kit ↔ PC qua RS232
- [x] Nhận trạng thái cảm biến
- [x] Theo dõi trạng thái slot
- [x] Quản lý bộ đếm xe vào/ra
- [x] Tính số vị trí còn trống
- [x] Lưu dữ liệu bằng SQLite
- [x] Ghi nhận lịch sử sự kiện
- [x] Web Dashboard giám sát bãi xe
- [x] Đồng bộ dữ liệu Kit → Python → Web
- [x] Hiển thị trạng thái cổng và slot
- [ ] Tích hợp hoàn chỉnh ANPR

---

## 🛠️ Công nghệ sử dụng

- **C / Embedded C** – lập trình STM32/ARM Kit
- **STM32** – bộ điều khiển chính
- **RS232** – giao tiếp nối tiếp
- **Python** – xử lý dữ liệu phía máy tính
- **SQLite** – cơ sở dữ liệu
- **Flask** – Web Backend
- **HTML / CSS / JavaScript** – Web Dashboard
- **ANPR** – định hướng nhận diện biển số

---

## 📁 Repository

Repository này là nơi lưu trữ tài liệu và mã nguồn của dự án Smart Parking IoT khi được đưa lên GitHub.

Cấu trúc và thành phần repository có thể được mở rộng theo từng giai đoạn phát triển của dự án.

---

## 🚀 Hướng phát triển

- Hoàn thiện tích hợp ANPR.
- Liên kết biển số với lịch sử xe vào/ra.
- Cải thiện giao diện Web Dashboard.
- Bổ sung thống kê và báo cáo hoạt động của bãi xe.
- Tăng khả năng giám sát trạng thái hệ thống theo thời gian thực.
- Hoàn thiện tài liệu kỹ thuật và quy trình triển khai.

---

## 👨‍💻 Tác giả

**Nguyễn Ngọc Hùng**  
Sinh viên Điện tử – Viễn thông, IUH.

---

## 📌 Trạng thái dự án

**Smart Parking IoT – đang phát triển**

Dự án tập trung vào việc kết nối **hệ thống nhúng + giao tiếp truyền thông + xử lý dữ liệu + cơ sở dữ liệu + Web Dashboard** thành một hệ thống bãi đỗ xe thông minh hoàn chỉnh.
