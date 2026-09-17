@echo off
setlocal
title Smart Parking IoT - Setup

cd /d "%~dp0"

 echo ==========================================
echo       SMART PARKING IOT - SETUP
echo ==========================================
echo.

echo [1] Checking Python 3.11...
py -3.11 --version
if errorlevel 1 (
    echo Python 3.11 was not found.
    pause
    exit /b 1
)

echo.
echo [2] Creating virtual environment...
if not exist ".venv\Scripts\python.exe" py -3.11 -m venv .venv
if errorlevel 1 (
    echo Failed to create virtual environment.
    pause
    exit /b 1
)

echo.
echo [3] Upgrading pip...
".venv\Scripts\python.exe" -m pip install --upgrade pip
if errorlevel 1 (
    echo Failed to upgrade pip.
    pause
    exit /b 1
)

echo.
echo [4] Installing Python dependencies...
if exist "Core\Python\requirements.txt" ".venv\Scripts\python.exe" -m pip install -r "Core\Python\requirements.txt"
if errorlevel 1 (
    echo Failed to install Core/Python requirements.
    pause
    exit /b 1
)
if exist "Core\Python\web\requirements_web.txt" ".venv\Scripts\python.exe" -m pip install -r "Core\Python\web\requirements_web.txt"
if errorlevel 1 (
    echo Failed to install Web requirements.
    pause
    exit /b 1
)
if exist "Core\Python\ANPR\requirements_anpr.txt" ".venv\Scripts\python.exe" -m pip install -r "Core\Python\ANPR\requirements_anpr.txt"
if errorlevel 1 (
    echo Failed to install ANPR requirements.
    pause
    exit /b 1
)

echo.
echo [5] Checking ANPR model...
if exist "Core\Python\ANPR\models\best.pt" (
    echo OK - best.pt found.
) else (
    echo WARNING - best.pt is not installed.
    echo Place best.pt at:
    echo Core\Python\ANPR\models\best.pt
)

echo.
echo ==========================================
echo Setup completed.
echo ==========================================
echo.
echo Run:
echo   run_main.bat
echo   run_anpr.bat
echo   run_web.bat
echo.
pause
