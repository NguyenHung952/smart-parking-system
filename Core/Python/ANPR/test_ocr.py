import cv2

from camera import ParkingCamera
from detector import LicensePlateDetector
from ocr import PlateOCR


def main():
    camera = ParkingCamera(
        camera_index=0,
        width=1280,
        height=720,
        fps=15,
    )

    detector = LicensePlateDetector(
        model_path="models/best.pt",
        confidence=0.35,
        image_size=640,
        device="cpu",
    )

    ocr = PlateOCR()

    print("[ANPR] YOLO + PaddleOCR đang chạy.")
    print("[ANPR] Đưa hình biển số trước camera.")
    print("[ANPR] Nhấn Q hoặc ESC để thoát.")

    try:
        camera.open()

        while True:
            frame = camera.read()

            annotated, crops, detections = detector.detect(frame)

            for i, crop in enumerate(crops):
                result = ocr.recognize(crop)

                if result.text:
                    text = result.text
                    confidence = result.confidence

                    print(
                        f"[OCR] Plate {i}: "
                        f"{text} "
                        f"(confidence={confidence:.2f})"
                    )

                    cv2.putText(
                        annotated,
                        text,
                        (20, 70 + i * 35),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.9,
                        (0, 255, 0),
                        2,
                    )

            cv2.putText(
                annotated,
                f"License plates: {len(detections)}",
                (20, 35),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.8,
                (0, 255, 0),
                2,
            )

            cv2.imshow(
                "Smart Parking - ANPR YOLO + OCR",
                annotated,
            )

            key = cv2.waitKey(1) & 0xFF

            if key in (ord("q"), 27):
                break

    finally:
        camera.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()