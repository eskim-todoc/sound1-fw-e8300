from __future__ import annotations

import os
import zipfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


TASK = Path(__file__).resolve().parents[1]
DOCX = TASK / "tmp" / "reference.docx"
OUT = TASK / "tmp" / "reference-media"
CONTACT = TASK / "tmp" / "reference-media-contact.png"


def font(size: int):
    path = Path("C:/Windows/Fonts/malgun.ttf")
    return ImageFont.truetype(str(path), size) if path.exists() else ImageFont.load_default()


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(DOCX) as zf:
        for name in zf.namelist():
            if name.startswith("word/media/") and not name.endswith("/"):
                (OUT / Path(name).name).write_bytes(zf.read(name))

    entries = []
    for p in sorted(OUT.iterdir(), key=lambda x: x.name.lower()):
        try:
            im = Image.open(p).convert("RGB")
            entries.append((p, im.copy()))
        except Exception:
            continue

    cell_w, cell_h = 420, 320
    cols = 4
    rows = (len(entries) + cols - 1) // cols
    canvas = Image.new("RGB", (cell_w * cols, cell_h * rows), "white")
    draw = ImageDraw.Draw(canvas)
    label_font = font(19)

    for idx, (path, im) in enumerate(entries):
        x = (idx % cols) * cell_w
        y = (idx // cols) * cell_h
        draw.rectangle((x, y, x + cell_w - 1, y + cell_h - 1), outline="#888888", width=1)
        draw.text((x + 10, y + 8), f"{path.name}  {im.width}x{im.height}", fill="black", font=label_font)
        max_w, max_h = cell_w - 20, cell_h - 48
        im.thumbnail((max_w, max_h), Image.Resampling.LANCZOS)
        px = x + (cell_w - im.width) // 2
        py = y + 40 + (max_h - im.height) // 2
        canvas.paste(im, (px, py))

    canvas.save(CONTACT)
    print(f"extracted={len(entries)}")
    print(CONTACT)


if __name__ == "__main__":
    main()
