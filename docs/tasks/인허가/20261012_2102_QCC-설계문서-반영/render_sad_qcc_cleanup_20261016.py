"""Render isolated read-only Word previews; no acceptance is saved to deliverable."""
from pathlib import Path
import json, sys
import subprocess
import pypdfium2 as pdfium

QA=Path(__file__).resolve().parent/'qa/sad-qcc-cleanup-20261016'
def render():
    subprocess.run(['pwsh','-NoProfile','-File',str(Path(__file__).with_suffix('.ps1'))],check=True)
    pdf=pdfium.PdfDocument(QA/'candidate.pdf')
    out=QA/'pages';out.mkdir(exist_ok=True)
    for i in range(len(pdf)):
        page=pdf[i];img=page.render(scale=1.5).to_pil();img.save(out/f'page-{i+1:03}.png')
    # Rejected-new preview must match the original's accepted visible body.
    a=(QA/'reject-new-word.txt').read_text(encoding='utf-8')
    b=(QA/'before-word.txt').read_text(encoding='utf-8')
    import re
    norm=lambda s:re.sub(r'\s+','',s)
    ok=norm(a)==norm(b)
    (QA/'word-render-check.json').write_text(json.dumps({'pages':len(pdf),'reject_new_matches_before_after_word_accept':ok},indent=2),encoding='utf-8')
    print('Word rejection match',ok,'pages',len(pdf),flush=True)
    assert ok

if __name__=='__main__':render()
