import argparse
from pathlib import Path
import qrcode


def main():
    parser = argparse.ArgumentParser(description="Generate Smart Parking QR")
    parser.add_argument("qr_id")
    parser.add_argument("--output", default=None)
    args = parser.parse_args()

    qr_id = str(args.qr_id).strip().upper()
    if not qr_id:
        raise SystemExit("qr_id không được rỗng")

    if args.output:
        out = Path(args.output)
        if out.suffix.lower() != ".png":
            out = out / f"{qr_id}.png"
    else:
        out = Path(__file__).resolve().parent / "qr_cards" / f"{qr_id}.png"

    out.parent.mkdir(parents=True, exist_ok=True)
    img = qrcode.make(qr_id)
    img.save(out)
    print(f"Da tao: {out}")


if __name__ == "__main__":
    main()
