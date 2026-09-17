# Smart Parking IoT

Smart Parking IoT using STM32F207 ARM KIT, Python, Web Dashboard, QR and ANPR.

## Project structure

- Core/ - STM32 application source code
- Drivers/ - STM32 HAL and CMSIS
- Core/Python/ - Python backend
- Core/Python/ANPR/ - license plate recognition
- Core/Python/web/ - Web Dashboard

## Hardware

- STM32F207VGTX ARM KIT
- LCD 16x2
- 4 parking slots
- RS232 communication
- 24C16 EEPROM

## Software requirements

- STM32CubeIDE
- Python 3.11
- Git

## ANPR model

The ANPR model file best.pt is not included in this repository.

Download the model separately and place it at:

Core/Python/ANPR/models/best.pt

See Core/Python/ANPR/models/README.md for details.

## Python environment

The .venv directory is not included in the repository.
Create a new Python virtual environment and install the required packages from the requirements files.

## Runtime data

Database files, logs, build output and generated files are not included in the repository.

## Run

STM32:
Open the project with STM32CubeIDE and build/flash the firmware.

Python / Main:
run_main.bat

ANPR:
run_anpr.bat

Web Dashboard:
run_web.bat

## Notes

This repository contains the project source and configuration files.
