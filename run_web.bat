@echo off
setlocal
cd /d "%~dp0"

echo ============================================================
echo   SMART PARKING - FLASK WEB DASHBOARD
echo ============================================================
echo.
echo Thu muc Web: %CD%
echo Python: %~dp0..\..\.venv\Scripts\python.exe

echo.
if not exist "%~dp0..\..\.venv\Scripts\python.exe" (
    echo [ERROR] Khong tim thay Python .venv.
    echo Kiem tra: D:\STM32CUBE\BAITAP\parking_iot\.venv\Scripts\python.exe
    pause
    exit /b 1
)

"%~dp0..\..\.venv\Scripts\python.exe" app.py

echo.
echo Flask da dung. Ma loi: %ERRORLEVEL%
pause
