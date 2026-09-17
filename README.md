# \# Smart Parking IoT

# 

# Hệ thống bãi đỗ xe thông minh sử dụng STM32F207 ARM KIT, Python, SQLite, Web Dashboard, QR và ANPR.

# 

# \## 1. Kiến trúc hệ thống

# 

# ```text

# STM32 / ARM KIT

# &#x20;      |

# &#x20;    RS232

# &#x20;      |

# &#x20;      v

# Python Application

# &#x20;      |

# &#x20;      v

# SQLite Database

# &#x20;      |

# &#x20;      v

# Web Dashboard

# 

# STM32/ARM KIT là bộ điều khiển chính của hệ thống. Python nhận dữ liệu từ Kit, xử lý và đồng bộ dữ liệu với cơ sở dữ liệu và Web Dashboard.

# 

# 2\. Thành phần phần cứng

# STM32F207VGTX ARM KIT

# LCD 16x2

# 4 parking slots

# EEPROM 24C16

# RS232

# Nút/cảm biến mô phỏng trạng thái slot và cổng

# 3\. Công nghệ

# Embedded C / STM32 HAL

# STM32F207

# STM32CubeIDE

# RS232

# Python 3.11

# SQLite

# Flask

# HTML / CSS / JavaScript

# ANPR

# QR

# 4\. Cấu trúc project

# parking\_iot/

# |

# +-- Core/

# |   +-- Inc/

# |   +-- Src/

# |   +-- Startup/

# |   +-- Python/

# |       +-- ANPR/

# |       +-- web/

# |

# +-- Drivers/

# |

# +-- parking\_iot.ioc

# +-- .project

# +-- .cproject

# +-- setup.bat

# +-- run\_main.bat

# +-- run\_anpr.bat

# +-- run\_web.bat

# +-- README.md

# 5\. Yêu cầu phần mềm

# 

# Cần cài:

# 

# Git

# Python 3.11

# STM32CubeIDE

# Driver/USB hoặc giao tiếp phù hợp với ARM KIT

# 

# Kiểm tra Python:

# 

# py -3.11 --version

# 6\. Cài đặt project

# 

# Clone repository:

# 

# git clone https://github.com/NguyenHung952/smart-parking-system.git

# 

# Vào project:

# 

# cd smart-parking-system

# 

# Chạy:

# 

# setup.bat

# 

# Script sẽ:

# 

# Kiểm tra Python 3.11.

# Tạo Python virtual environment.

# Cập nhật pip.

# Cài các package từ các file requirements.

# Kiểm tra ANPR model.

# 7\. ANPR model

# 

# Model best.pt không được lưu trực tiếp trong Git repository.

# 

# File cần có:

# 

# Core/Python/ANPR/models/best.pt

# 

# Tải model từ nguồn tài nguyên của project và đặt đúng tên:

# 

# best.pt

# 

# Sau khi đặt file, cấu trúc phải là:

# 

# Core/

# └── Python/

# &#x20;   └── ANPR/

# &#x20;       └── models/

# &#x20;           ├── best.pt

# &#x20;           └── README.md

# 8\. Chạy STM32

# 

# Mở project:

# 

# parking\_iot.ioc

# 

# hoặc import project vào STM32CubeIDE.

# 

# Build firmware và nạp firmware vào STM32F207 ARM KIT.

# 

# 9\. Chạy Python

# 

# Có thể chạy:

# 

# run\_main.bat

# 10\. Chạy ANPR

# 

# Sau khi đã cài model:

# 

# run\_anpr.bat

# 

# ANPR sử dụng camera và model nhận diện biển số.

# 

# 11\. Chạy Web Dashboard

# run\_web.bat

# 

# Web Dashboard được sử dụng để theo dõi:

# 

# Trạng thái 4 slot

# Số xe đang có trong bãi

# Số chỗ còn trống

# Bộ đếm IN/OUT

# Trạng thái cổng

# Lịch sử sự kiện

# Thông tin ANPR

# 12\. Database

# 

# Database SQLite được tạo và sử dụng trong quá trình chạy hệ thống.

# 

# Các database runtime không được đưa vào repository.

# 

# Repository chỉ chứa source code và cấu hình cần thiết.

# 

# 13\. Các file không nằm trong Git

# 

# Các dữ liệu runtime/build không được commit:

# 

# .venv/

# Debug/

# build/

# dist/

# logs/

# \*.db

# \*.db-wal

# \*.db-shm

# SmartParking.exe

# best.pt

# 14\. Chức năng hiện tại

# &#x20;STM32/ARM KIT

# &#x20;4 parking slots

# &#x20;LCD 16x2

# &#x20;EEPROM 24C16

# &#x20;RS232 communication

# &#x20;Parking state management

# &#x20;Vehicle IN/OUT counter

# &#x20;SQLite database

# &#x20;Web Dashboard

# &#x20;QR processing

# &#x20;ANPR source integration

# &#x20;Hoàn thiện toàn bộ quy trình ANPR thực tế

# &#x20;Gate motor integration

# &#x20;RFID integration

# 15\. Lưu ý

# 

# Project sử dụng ARM KIT làm nguồn trạng thái chính của hệ thống parking.

# 

# Python và Web Dashboard nhận và hiển thị dữ liệu từ hệ thống.

# 

# Không nên đưa các file runtime, virtual environment hoặc file build vào Git repository.

# 

# 16\. Tác giả

# 

# Nguyễn Ngọc Hùng

# 

# Sinh viên Điện tử - Viễn thông, IUH.

# 

# 17\. Repository

# 

# https://github.com/NguyenHung952/smart-parking-system

