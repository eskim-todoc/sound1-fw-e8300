"""Read original/current documents and keep the reconciliation baseline immutable."""
from pathlib import Path
from collections import Counter
import sys,json,zipfile,re,hashlib,shutil
from lxml import etree as E
import openpyxl
T=Path(__file__).resolve().parent
ROOT=T.parents[3]
OUT=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어'
QA=T/'qa/unit-reconciliation'
N={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
def visible(p):return ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=N))
def paras(path):
    with zipfile.ZipFile(path) as z:r=E.fromstring(z.read('word/document.xml'))
    return [{'index':i,'text':visible(p),'style':p.xpath('./w:pPr/w:pStyle/@w:val',namespaces=N)} for i,p in enumerate(r.xpath('//w:body/w:p',namespaces=N))]
def main():
    QA.mkdir(parents=True,exist_ok=True)
    inv=json.loads((T/'inventory.json').read_text(encoding='utf8'))
    data={}
    for kind in ['SDDD','SUVC','SAD']:
        it=next(x for x in inv if x['name'].startswith(kind+'-'))
        src=Path(it['source']); live=next(OUT.glob(kind+'*.docx' if kind!='SUVC' else kind+'*.xlsx'))
        for label,p in [('source',src),('before',live)]:
            dest=QA/(kind+'-'+label+p.suffix)
            if not dest.exists():shutil.copy2(p,dest)
            else:assert dest.read_bytes()==p.read_bytes(),str(p)
            data[kind+'-'+label]={'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
            if p.suffix=='.docx':data[kind+'-'+label]['paragraphs']=paras(p)
            else:
                w=openpyxl.load_workbook(p,data_only=True)
                data[kind+'-'+label]['rows']=[[c.value for c in row] for row in w.worksheets[1]]
    sys.path.insert(0,str(ROOT/'docs/tasks/인허가/20260909_1734_suvr-sound1/스크립트'))
    import suvc_rows
    data['supplement_units']=suvc_rows.build()
    (QA/'baseline.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf8')
    for label in ['SDDD-source','SDDD-before']:
        print(label)
        for p in data[label]['paragraphs']:
            if re.match(r'\s*(?:SI|SU)-ID-',p['text']): print(p['text'])
    print('supplement count',len(data['supplement_units']))
if __name__=='__main__':main()
