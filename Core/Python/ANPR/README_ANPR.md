# ANPR - Smart Parking

Folder này được tách riêng để phát triển nhận diện biển số bằng camera PC, không thay đổi firmware STM32.

## Kiến trúc

Camera Lenovo T470
-> OpenCV
-> YOLO license-plate detector
-> crop biển số
-> PaddleOCR
-> chuẩn hóa biển số Việt Nam
-> `parking_history.db`
-> Web Dashboard (ở bước tích hợp sau)

## Các file

- `camera.py`: mở webcam và kiểm tra camera.
- `detector.py`: YOLO phát hiện vùng biển số.
- `ocr.py`: PaddleOCR đọc ký tự và chuẩn hóa chuỗi.
- `plate_history.py`: lưu lịch sử ANPR vào bảng riêng `license_plate_history`.
- `models/`: nơi đặt model YOLO biển số (`best.pt`).

## Nguồn tham khảo

Thiết kế được nghiên cứu từ các repository người dùng cung cấp:
- cuongle4399/VietnamLicensePlateRecognition
- phatnomenal/Vietnamese-License-Plate-Detecttion
- trungdinh22/License-Plate-Recognition
- cnmeow/VNPlateRec
- itsZiang/vietnamese-license-plate-recognition
- max-tan/LicensePlate
- Ali-Kalsekar/Automatic-Number-Plate-Recognition-System

Không sao chép nguyên project vào Smart Parking. Các module được tách lại để phù hợp với kiến trúc hiện tại.

## Quan trọng về model

`yolov8n.pt` thông thường là model YOLO tổng quát và không tự động trở thành model nhận diện biển số. Cần một `.pt` đã được train/fine-tune cho license plate.

Đặt model tại:

`ANPR/models/best.pt`

hoặc truyền đường dẫn model riêng khi tích hợp.

## Test camera trước

Từ thư mục `Core/Python/ANPR`:

`python camera.py --camera 0`

Nếu không mở được webcam tích hợp, thử:

`python camera.py --camera 1`

## Test database

`python plate_history.py`

Module sẽ chỉ tạo bảng `license_plate_history`; không sửa các bảng `status_history` và `parking_events`.

## Giai đoạn tích hợp tiếp theo

Chưa tự động nối ANPR vào `main.py` hoặc Web Dashboard ở bản này. Sau khi camera + detector + OCR chạy ổn định, mới tích hợp theo pipeline:

Camera -> ANPR -> plate_history -> sự kiện IN/OUT -> Web API -> Dashboard.

Điều này giữ phần STM32/RS232 hiện tại độc lập trong giai đoạn thử nghiệm.
