import io
import sys
import zipfile
from pathlib import Path

from lxml import etree
from PIL import Image


NS = {
    "a": "http://schemas.openxmlformats.org/drawingml/2006/main",
    "a14": "http://schemas.microsoft.com/office/drawing/2010/main",
    "pr": "http://schemas.openxmlformats.org/package/2006/relationships",
}


def optimized_png(raw: bytes) -> bytes:
    image = Image.open(io.BytesIO(raw))
    if max(image.size) > 1800:
        ratio = 1800 / max(image.size)
        image = image.resize(
            (round(image.width * ratio), round(image.height * ratio)),
            Image.Resampling.LANCZOS,
        )
    if image.mode == "RGBA":
        background = Image.new("RGB", image.size, "white")
        background.paste(image, mask=image.getchannel("A"))
        image = background
    elif image.mode != "RGB":
        image = image.convert("RGB")
    out = io.BytesIO()
    image.save(out, format="PNG", optimize=True)
    return out.getvalue()


def placeholder_image(name: str) -> bytes:
    if name.endswith(".svg"):
        return (
            b'<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">'
            b'<rect width="100" height="100" fill="white"/></svg>'
        )
    image = Image.new("RGB", (100, 100), "white")
    out = io.BytesIO()
    image.save(out, format="JPEG" if name.endswith((".jpg", ".jpeg")) else "PNG")
    return out.getvalue()


def sanitize(source: Path, target: Path, blank_images: bool = False) -> None:
    with zipfile.ZipFile(source, "r") as zin, zipfile.ZipFile(
        target, "w", zipfile.ZIP_DEFLATED
    ) as zout:
        for item in zin.infolist():
            name = item.filename
            if name.endswith(".wdp"):
                continue
            raw = zin.read(name)
            if name == "word/document.xml":
                root = etree.fromstring(raw)
                for ext in root.xpath("//a:ext[a14:imgProps]", namespaces=NS):
                    ext.getparent().remove(ext)
                raw = etree.tostring(root, xml_declaration=True, encoding="UTF-8", standalone=True)
            elif name == "word/_rels/document.xml.rels":
                root = etree.fromstring(raw)
                for rel in root.xpath("//pr:Relationship", namespaces=NS):
                    if rel.get("Type", "").endswith("/hdphoto"):
                        rel.getparent().remove(rel)
                raw = etree.tostring(root, xml_declaration=True, encoding="UTF-8", standalone=True)
            elif name == "[Content_Types].xml":
                root = etree.fromstring(raw)
                for node in root.xpath("//*[local-name()='Default'][@Extension='wdp']"):
                    node.getparent().remove(node)
                raw = etree.tostring(root, xml_declaration=True, encoding="UTF-8", standalone=True)
            elif blank_images and name.startswith("word/media/"):
                raw = placeholder_image(name)
            elif name == "word/media/image18.png":
                raw = optimized_png(raw)
            zout.writestr(item, raw)


def main() -> None:
    source = Path(sys.argv[1])
    target = Path(sys.argv[2])
    sanitize(source, target, "--blank-images" in sys.argv[3:])
    print(f"{source.stat().st_size} -> {target.stat().st_size}")


if __name__ == "__main__":
    main()
