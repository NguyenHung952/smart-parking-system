import cv2

from camera import ParkingCamera
from detector import LicensePlateDetector


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

    print("[ANPR] Camera + YOLO đang chạy.")
    print("[ANPR] Đưa biển số xe vào trước camera.")
    print("[ANPR] Nhấn Q hoặc ESC để thoát.")

    try:
        camera.open()

        while True:
            frame = camera.read()

            annotated, crops, detections = detector.detect(frame)

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
                "Smart Parking - YOLO License Plate",
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