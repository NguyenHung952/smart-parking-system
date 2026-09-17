# Smart Parking Web Dashboard

Dashboard đọc dữ liệu từ SQLite của PC Client.

## Giao diện hiện tại

- Giao diện quản lý đơn giản, nền sáng.
- Hiển thị Xe trong bãi, Chỗ trống, Lượt vào, Lượt ra và trạng thái cổng.
- Sơ đồ 4 SLOT chỉ hiển thị **TRỐNG / CÓ XE**, không hiển thị thanh biển số bên trong ô.
- Đã bỏ khối **Xe đang trong bãi / LIVE**.
- Lịch sử chỉ hiển thị các sự kiện xe **XE VÀO / XE RA**.

## Logic dữ liệu

Web chỉ đọc dữ liệu từ SQLite. Web không tự quyết định IN/OUT.

```text
ARM KIT -> RS232 -> Python Client -> SQLite -> Web Dashboard
```

IN/OUT phải do ARM KIT xác nhận bằng đúng quy trình CỔNG + SLOT.
