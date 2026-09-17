@echo off

cd /d D:\STM32CUBE\BAITAP\parking_iot

call .venv\Scripts\activate

cd /d D:\STM32CUBE\BAITAP\parking_iot\Core\Python\ANPR

python test_anpr.py

pause