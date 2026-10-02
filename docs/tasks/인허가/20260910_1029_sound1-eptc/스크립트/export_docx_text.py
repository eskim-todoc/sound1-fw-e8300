import sys
from pathlib import Path

from docx import Document
from docx.table import Table
from docx.text.paragraph import Paragraph


def iter_block_items(doc):
    for child in doc.element.body.iterchildren():
        if child.tag.endswith("}p"):
            yield Paragraph(child, doc)
        elif child.tag.endswith("}tbl"):
            yield Table(child, doc)


def export(source: Path, target: Path) -> None:
    doc = Document(str(source))
    lines = []
    p_no = 0
    t_no = 0
    for block in iter_block_items(doc):
        if isinstance(block, Paragraph):
            p_no += 1
            text = block.text.replace("\t", " <TAB> ").strip()
            if text:
                style = block.style.name if block.style else ""
                lines.append(f"[P{p_no:04d}][{style}] {text}")
        else:
            t_no += 1
            lines.append(f"[TABLE {t_no:02d} START]")
            for r_no, row in enumerate(block.rows, 1):
                cells = []
                for cell in row.cells:
                    cell_text = " / ".join(
                        p.text.replace("\t", " <TAB> ").strip()
                        for p in cell.paragraphs
                        if p.text.strip()
                    )
                    cells.append(cell_text)
                lines.append(f"[T{t_no:02d}R{r_no:03d}] " + " || ".join(cells))
            lines.append(f"[TABLE {t_no:02d} END]")
    target.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"paragraphs={p_no} tables={t_no} lines={len(lines)}")


if __name__ == "__main__":
    export(Path(sys.argv[1]), Path(sys.argv[2]))
