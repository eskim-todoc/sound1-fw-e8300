from pathlib import Path
from lxml import etree as E
import zipfile,json,hashlib
HERE=Path(__file__).resolve().parent
BASE=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기/서석일_작성')
out={}
for kind in ['SUVC','SDDD']:
 p=next(BASE.glob('QCC_'+kind+'*.docx'))
 with zipfile.ZipFile(p) as z: tree=E.fromstring(z.read('word/document.xml'))
 ns={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
 blocks=[]
 for i,b in enumerate(tree.find('w:body',ns),1):
  if b.tag.endswith('tbl'):
   blocks.append({'block':i,'rows':[[ '\n'.join(''.join(t.itertext()) for t in c.findall('.//w:t',ns)) for c in r.findall('w:tc',ns)] for r in b.findall('w:tr',ns)]})
  else: blocks.append({'block':i,'text':''.join(b.xpath('.//w:t/text()',namespaces=ns))})
 out[kind]={'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'blocks':blocks}
(HERE/'source-read.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(out,ensure_ascii=True))
