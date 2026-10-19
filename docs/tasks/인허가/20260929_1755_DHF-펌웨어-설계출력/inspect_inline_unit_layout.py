"""Inspect native PDF cell tails and render representative numerical blocks."""
import json
import re
import subprocess
from pathlib import Path
from pypdf import PdfReader
from inline_unit_acceptance import QA, save_json

POPPLER=Path('C:/Users/EUNSU/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/poppler/Library/bin/pdftoppm.exe')
plan=json.loads((QA/'plan.json').read_text('utf-8'))
native=json.loads((QA/'native-checks.json').read_text('utf-8-sig'))
assert all(not v['overflow'] for v in native),'Native AutoFit found clipped payload rows.'
def norm(value): return re.sub(r'\s+','',value).replace('−','-')
checks=[]
for key in ['criterion','report']:
    reader=PdfReader(QA/f'{key}-all.pdf')
    texts=[p.extract_text() or '' for p in reader.pages]
    joined=norm(''.join(texts));missing=[]
    for rec in plan['records']:
        for row in rec['rows']:
            for field in (['expected'] if key=='criterion' else ['expected','actual']):
                tail=norm(row[field])[-40:]
                if tail and tail not in joined:missing.append({'unit':rec['id'],'row':row['row'],'field':field,'tail':tail})
        for log in rec['sourceLogs']:
            for line in log[3].splitlines():
                if norm(line) not in joined:missing.append({'unit':rec['id'],'RTT':line})
    save_json(f'{key}-pdf-text.json',texts)
    checks.append({'key':key,'pages':len(texts),'missingCellTailsOrRtt':missing,
        'nativeAutoFitOverflow':0})
    for uid in ['CFX-001','CFX-020','CFX-023','HEAR-002','CM3-028','CM3-051','BOOT-018']:
        pdf=QA/f'{key}-{uid}-native.pdf'
        selected=[1,len(PdfReader(pdf).pages)]
        for page in sorted(set(selected)):
            output=QA/f'{key}-{uid}-native-{page}'
            subprocess.run([str(POPPLER),'-f',str(page),'-l',str(page),'-singlefile',
                '-scale-to','1600','-png',str(pdf),str(output)],check=True,capture_output=True)
save_json('layout-checks.json',checks)
print(json.dumps([{k:v for k,v in c.items()} for c in checks],ensure_ascii=False,indent=2))
