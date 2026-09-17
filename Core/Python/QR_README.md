# QR cho Smart Parking

Luồng QR tuần tự: ANPR đọc biển số trước -> dừng YOLO/OCR -> quét QR -> đối chiếu QR với biển số -> gửi OPEN_GATE_IN/OUT.

Lệnh chính:
- `qr_register CARD_001 "12-B1 168.88"`
- `qr_status`
- `qr_in`
- `qr_out`
- `qr_off`

STM32 không cần sửa.
