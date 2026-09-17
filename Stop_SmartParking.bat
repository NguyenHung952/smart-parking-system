@echo off
cd /d "%~dp0"
echo Stopping SmartParking...
type nul > "SmartParking.stop"
timeout /t 5 /nobreak >nul
del /q "SmartParking.stop" 2>nul
echo Done.
