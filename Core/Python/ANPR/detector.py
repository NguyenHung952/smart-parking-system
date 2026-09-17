"""
License-plate detector for the Smart Parking ANPR module.

Research basis:
- YOLOv8 + OpenCV pipeline from cuongle4399/VietnamLicensePlateRecognition
- YOLOv8 Vietnamese plate detection approach from phatnomenal/Vietnamese-License-Plate-Detecttion
- Two-stage ANPR architecture ideas from max-tan/LicensePlate

Important:
A generic yolov8n.pt model is NOT a license-plate detector. For useful ANPR,
set model_path to a YOLO model trained for Vietnamese license plates.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple

import cv2
import numpy as np

try:
    from ultralytics import YOLO
except ImportError:
    YOLO = None


@dataclass
class PlateDetection:
    box: Tuple[int, int, int, int]
    confidence: float
    class_id: int
    class_name: str


class LicensePlateDetector:
    """YOLO detector with safe path handling and CPU-friendly defaults."""

    def __init__(
        self,
        model_path: str | Path = "models/best.pt",
        confidence: float = 0.35,
        image_size: int = 640,
        device: str = "cpu",
    ) -> None:
        if YOLO is None:
            raise ImportError(
                "Thiếu ultralytics. Cài bằng: pip install ultralytics"
            )

        base = Path(__file__).resolve().parent
        path = Path(model_path)
        if not path.is_absolute():
            path = base / path

        self.model_path = path
        self.confidence = float(confidence)
        self.image_size = int(image_size)
        self.device = device

        if not self.model_path.exists():
            raise FileNotFoundError(
                f"Chưa có model biển số: {self.model_path}\n"
                "Hãy đặt model YOLO biển số Việt Nam vào ANPR/models/best.pt "
                "hoặc truyền --model tới file .pt."
            )

        self.model = YOLO(str(self.model_path))

    def detect(
        self,
        image: np.ndarray,
        confidence: float | None = None,
    ) -> tuple[np.ndarray, List[np.ndarray], List[PlateDetection]]:
        if image is None or image.size == 0:
            raise ValueError("Ảnh đầu vào rỗng.")

        threshold = self.confidence if confidence is None else float(confidence)
        result = self.model.predict(
            source=image,
            conf=threshold,
            imgsz=self.image_size,
            device=self.device,
            verbose=False,
        )[0]

        annotated = image.copy()
        crops: List[np.ndarray] = []
        detections: List[PlateDetection] = []

        names = result.names
        h, w = image.shape[:2]

        if result.boxes is None:
            return annotated, crops, detections

        for box in result.boxes:
            x1, y1, x2, y2 = map(int, box.xyxy[0].tolist())
            conf = float(box.conf[0])
            cls_id = int(box.cls[0])
            cls_name = names.get(cls_id, str(cls_id)) if isinstance(names, dict) else str(names[cls_id])

            x1, y1 = max(0, x1), max(0, y1)
            x2, y2 = min(w, x2), min(h, y2)
            if x2 <= x1 or y2 <= y1:
                continue

            # Small margin improves OCR when the model box is tight.
            mx = max(2, int((x2 - x1) * 0.05))
            my = max(2, int((y2 - y1) * 0.08))
            cx1, cy1 = max(0, x1 - mx), max(0, y1 - my)
            cx2, cy2 = min(w, x2 + mx), min(h, y2 + my)

            crop = image[cy1:cy2, cx1:cx2].copy()
            if crop.size == 0:
                continue

            detections.append(
                PlateDetection(
                    box=(x1, y1, x2, y2),
                    confidence=conf,
                    class_id=cls_id,
                    class_name=cls_name,
                )
            )
            crops.append(crop)

            cv2.rectangle(annotated, (x1, y1), (x2, y2), (0, 255, 0), 2)
            label = f"PLATE {conf:.2f}"
            cv2.putText(
                annotated,
                label,
                (x1, max(25, y1 - 8)),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.65,
                (0, 255, 0),
                2,
            )

        return annotated, crops, detections
