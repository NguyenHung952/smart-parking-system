"""
ANPR Camera - webcam capture for Smart Parking.

Designed for the built-in webcam of a Lenovo T470 or another USB webcam.
This module only handles camera capture. AI inference is kept in detector.py/ocr.py.
"""

from __future__ import annotations

import argparse
import time
from typing import Iterator, Optional

import cv2
import numpy as np


class ParkingCamera:
    """Simple OpenCV webcam wrapper."""

    def __init__(
        self,
        camera_index: int = 0,
        width: int = 1280,
        height: int = 720,
        fps: int = 15,
    ) -> None:
        self.camera_index = int(camera_index)
        self.width = int(width)
        self.height = int(height)
        self.fps = int(fps)
        self.cap: Optional[cv2.VideoCapture] = None

    def open(self) -> None:
        if self.cap is not None and self.cap.isOpened():
            return

        # CAP_DSHOW is useful on Windows; fall back to the default backend.
        cap = cv2.VideoCapture(self.camera_index, cv2.CAP_DSHOW)
        if not cap.isOpened():
            cap.release()
            cap = cv2.VideoCapture(self.camera_index)

        if not cap.isOpened():
            raise RuntimeError(
                f"Không mở được camera index={self.camera_index}. "
                "Thử camera_index=1 nếu máy có nhiều camera."
            )

        cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
        cap.set(cv2.CAP_PROP_FPS, self.fps)
        self.cap = cap

    def read(self) -> np.ndarray:
        if self.cap is None or not self.cap.isOpened():
            self.open()

        ok, frame = self.cap.read()
        if not ok or frame is None:
            raise RuntimeError("Không đọc được frame từ camera.")
        return frame

    def frames(self) -> Iterator[np.ndarray]:
        """Yield frames until the camera is stopped."""
        self.open()
        while self.cap is not None and self.cap.isOpened():
            ok, frame = self.cap.read()
            if not ok:
                break
            yield frame

    def release(self) -> None:
        if self.cap is not None:
            self.cap.release()
            self.cap = None


def preview_camera(camera_index: int = 0) -> None:
    """Standalone camera test. Press Q or ESC to exit."""
    camera = ParkingCamera(camera_index=camera_index)
    try:
        camera.open()
        print(f"[ANPR] Camera index={camera_index} đang chạy. Nhấn Q/ESC để thoát.")

        last = time.perf_counter()
        frames = 0
        fps = 0.0

        while True:
            frame = camera.read()
            frames += 1
            now = time.perf_counter()
            if now - last >= 1.0:
                fps = frames / (now - last)
                frames = 0
                last = now

            cv2.putText(
                frame,
                f"ANPR Camera | FPS: {fps:.1f}",
                (20, 35),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.8,
                (0, 255, 0),
                2,
            )
            cv2.imshow("Smart Parking - ANPR Camera", frame)

            key = cv2.waitKey(1) & 0xFF
            if key in (ord("q"), 27):
                break
    finally:
        camera.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Test webcam for Smart Parking ANPR")
    parser.add_argument("--camera", type=int, default=0, help="OpenCV camera index")
    args = parser.parse_args()
    preview_camera(args.camera)
