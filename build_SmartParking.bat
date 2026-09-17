@echo off
setlocal
cd /d "%~dp0"

if not exist ".venv\Scripts\python.exe" (
    echo ERROR: .venv Python not found.
    pause
    exit /b 1
)

echo Building SmartParking V5...
".venv\Scripts\python.exe" -m PyInstaller --clean --noconfirm SmartParking.spec

if errorlevel 1 (
    echo.
    echo BUILD FAILED.
    pause
    exit /b 1
)

copy /y "dist\SmartParking.exe" "SmartParking.exe" >nul

echo.
echo BUILD OK: SmartParking.exe
pause
