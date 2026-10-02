from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


TASK = Path(__file__).resolve().parents[1]
OUT = TASK / "그림"
OUT.mkdir(parents=True, exist_ok=True)


def get_font(size, bold=False):
    name = "malgunbd.ttf" if bold else "malgun.ttf"
    return ImageFont.truetype(str(Path("C:/Windows/Fonts") / name), size)


def centered(draw, box, text, font, fill="#102A43"):
    x1, y1, x2, y2 = box
    bb = draw.multiline_textbbox((0, 0), text, font=font, spacing=7, align="center")
    w, h = bb[2] - bb[0], bb[3] - bb[1]
    draw.multiline_text(((x1 + x2 - w) / 2, (y1 + y2 - h) / 2), text, font=font, fill=fill, spacing=7, align="center")


def arrow(draw, x1, y, x2, color="#486581"):
    draw.line((x1, y, x2, y), fill=color, width=5)
    draw.polygon([(x2, y), (x2 - 16, y - 10), (x2 - 16, y + 10)], fill=color)


def main():
    canvas = Image.new("RGB", (1800, 500), "white")
    d = ImageDraw.Draw(canvas)
    title = get_font(34, True)
    body = get_font(26, True)
    sub = get_font(20)
    d.text((55, 28), "Sound1 N-of-M 필수성능 시험의 관측 경로", font=title, fill="#102A43")

    labels = [
        ("2-DMIC 입력", "Bluetooth Classic\n스트리밍 비활성"),
        ("빔포밍·AGC", "CFX 시간영역 처리"),
        ("512점 FFT", "31.25 Hz/bin"),
        ("32대역 대표값", "대역별 최대 magnitude"),
        ("상위 16개 선택", "동률이면 저주파 우선"),
        ("PCM 자극 출력", "2 ms에 16채널 관측"),
    ]
    left, top, bw, bh, gap = 45, 145, 245, 245, 50
    boxes = []
    colors = ["#EAF4FF", "#EAF4FF", "#F2F7FF", "#F2F7FF", "#E8F7EE", "#FFF4E5"]
    for i, ((name, detail), bg) in enumerate(zip(labels, colors)):
        x1 = left + i * (bw + gap)
        box = (x1, top, x1 + bw, top + bh)
        boxes.append(box)
        d.rounded_rectangle(box, radius=18, fill=bg, outline="#829AB1", width=3)
        centered(d, (x1 + 10, top + 20, x1 + bw - 10, top + 112), name, body)
        d.line((x1 + 24, top + 125, x1 + bw - 24, top + 125), fill="#BCCCDC", width=2)
        centered(d, (x1 + 10, top + 135, x1 + bw - 10, top + bh - 16), detail, sub, "#334E68")
        if i:
            arrow(d, boxes[i - 1][2] + 5, top + bh // 2, x1 - 8)

    path = OUT / "그림_NofM_관측경로.png"
    canvas.save(path)
    print(path)


if __name__ == "__main__":
    main()
