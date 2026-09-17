@echo off

cd /d D:\STM32CUBE\BAITAP\parking_iot

call .venv\Scripts\activate

cd /d D:\STM32CUBE\BAITAP\parking_iot\Core\Python

python main.py

pause