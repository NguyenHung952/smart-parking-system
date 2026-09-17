from dataclasses import dataclass
import re

from paddleocr import PaddleOCR


@dataclass
class OCRResult:
    text: str
    confidence: float


class PlateOCR:
    def __init__(self):
        self.ocr = PaddleOCR(
            lang="en",
            use_doc_orientation_classify=False,
            use_doc_unwarping=False,
            use_textline_orientation=False,
            device="cpu",
            engine="paddle_dynamic",
        )

    @staticmethod
    def normalize_plate(text):
        if not text:
            return ""

        text = text.upper().strip()

        # Chuẩn hóa ký tự phân cách
        text = text.replace(" ", "")
        text = text.replace("_", "-")
        text = text.replace(".", "-")

        # Chỉ giữ chữ cái, số và dấu '-'
        text = re.sub(r"[^A-Z0-9-]", "", text)

        # Gộp nhiều dấu '-' liên tiếp
        text = re.sub(r"-+", "-", text)

        # Xóa dấu '-' ở đầu/cuối
        text = text.strip("-")

        return text

    def recognize(self, image):
        if image is None:
            return OCRResult("", 0.0)

        result = self.ocr.predict(image)

        texts = []
        scores = []

        for res in result:
            data = getattr(res, "json", None)

            if callable(data):
                data = data()

            if not data:
                continue

            if isinstance(data, dict):
                data = data.get("res", data)

            rec_texts = data.get("rec_texts", [])
            rec_scores = data.get("rec_scores", [])

            texts.extend(rec_texts)
            scores.extend(rec_scores)

        if not texts:
            return OCRResult("", 0.0)

        # Ghép các kết quả OCR bằng dấu '-'
        cleaned_texts = []

        for t in texts:
            cleaned = self.normalize_plate(str(t))

            if cleaned:
                cleaned_texts.append(cleaned)

        text = "-".join(cleaned_texts)

        confidence = 0.0

        if scores:
            try:
                confidence = sum(
                    float(s) for s in scores
                ) / len(scores)
            except (TypeError, ValueError):
                confidence = 0.0

        return OCRResult(text, confidence)
