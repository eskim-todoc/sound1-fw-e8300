"""Read-only reference extraction and pre-edit snapshots for cyber system tests."""
from pathlib import Path
import hashlib, json, re, shutil, zipfile
from lxml import etree as E
from pypdf import PdfReader

TASK = Path(__file__).resolve().parent
ROOT = TASK.parents[3]
QA = TASK / 'qa/system-cybersecurity-rev0'
REF = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/04_docs/99_참고용DHF/04_설계 검증/SW')
DHF = ROOT / 'docs/참고/인허가/사운드1/DHF'
NS = {'s':'http://schemas.openxmlformats.org/spreadsheetml/2006/main', 'r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()

def extract(p):
    out = {'path':str(p), 'sha256':sha(p), 'sheets':{}}
    with zipfile.ZipFile(p) as z:
        strings=[]
        if 'xl/sharedStrings.xml' in z.namelist():
            strings=[''.join(x.itertext()) if False else ''.join(x.xpath('.//s:t/text()',namespaces=NS)) for x in E.fromstring(z.read('xl/sharedStrings.xml'))]
        wb=E.fromstring(z.read('xl/workbook.xml'))
        rels={r.get('Id'):r.get('Target') for r in E.fromstring(z.read('xl/_rels/workbook.xml.rels'))}
        for s in wb.find('s:sheets',NS):
            target=rels[s.get('{'+NS['r']+'}id')]
            target=target.lstrip('/') if target.startswith('/') else 'xl/'+target
            tree=E.fromstring(z.read(target)); rows=[]; cells={}
            for row in tree.findall('s:sheetData/s:row',NS):
                rr={}
                for c in row:
                    a=c.get('r'); t=c.get('t'); v=c.find('s:v',NS)
                    val=strings[int(v.text)] if t=='s' and v is not None else ''.join(c.xpath('.//s:t/text()',namespaces=NS)) if t=='inlineStr' else (v.text if v is not None else '')
                    if val: rr[a]=val; cells[a]=val
                if rr: rows.append({'row':int(row.get('r')),'cells':rr})
            out['sheets'][s.get('name')]={'part':target,'rows':rows,'cells':cells,'merges':[m.get('ref') for m in tree.findall('s:mergeCells/s:mergeCell',NS)],'dimension':tree.find('s:dimension',NS).get('ref') if tree.find('s:dimension',NS) is not None else None}
    return out

def main():
    QA.mkdir(parents=True, exist_ok=True)
    source={
      'checklist':REF/'[1.5세대] 사이버보안체크리스트-펌웨어_2차보완_최종1 by 김은수.xlsx',
      'svvr':REF/'[1.5세대] 나-1. SVVR-FW-TD20B소프트웨어 검증 및 유효성 확인 보고서(펌웨어)_2차보완_최종_1 Rev.2 by 김은수(수정완료).xlsx',
      'legacySystem':REF/'펌웨어/1-다. SSTR-FW-TD23D 소프트웨어 시스템 시험 보고서 Rev.1.xlsx',
      'system':DHF/'04_설계 검증/SW/펌웨어/SSTR-FW-TD24A 소프트웨어 시스템 시험 보고서_rev0(김은수 작성).xlsx',
      'oldCriterion':DHF/'03_설계 출력/SW/펌웨어/[rev0 old] SSTC-FW-TD24A 소프트웨어 시스템 시험 기준서.xlsx',
      'unit':DHF/'04_설계 검증/SW/펌웨어/SUVR-FW-TD24A 소프트웨어 유닛 검증 보고서_rev0(김은수 작성).xlsx',
    }
    baseline={'sources':{},'protected':{}}
    for key,p in source.items():
        x=extract(p)
        (QA/(key+'.json')).write_text(json.dumps(x,ensure_ascii=False,indent=2),encoding='utf-8')
        baseline['sources'][key]={'path':str(p),'sha256':x['sha256']}
        if key in ['system','oldCriterion']:
            backup=QA/(key+'-before.xlsx')
            if backup.exists(): assert sha(backup)==x['sha256'], 'Baseline already exists with different data'
            else: shutil.copy2(p,backup)
            baseline['sources'][key]['backup']=str(backup)
        print(key,[(n,s['dimension'],len(s['cells'])) for n,s in x['sheets'].items()])
    for p in DHF.rglob('*'):
        if p.is_file() and p.suffix in ['.xlsx','.docx'] and (any(v in p.name for v in ['SUVC-','SUVR-','SITC-','SITR-','EPTC-','작성자 전용'])):
            baseline['protected'][str(p)]=sha(p)
    for p in (ROOT/'src').rglob('*'):
        if p.is_file() and p.suffix.lower() in ['.c','.h','.hct']:
            baseline['protected'][str(p)]=sha(p)
    pdf=next(p for p in REF.glob('*.pdf') if '의료기기의+사이버보안' in p.name)
    baseline['sources']['guideline']={'path':str(pdf),'sha256':sha(pdf)}
    d=PdfReader(pdf)
    (QA/'guideline.txt').write_text('\n\n'.join(f'--- PDF page {i+1} ---\n'+(p.extract_text() or '') for i,p in enumerate(d.pages)),encoding='utf-8')
    (QA/'baseline.json').write_text(json.dumps(baseline,ensure_ascii=False,indent=2),encoding='utf-8')
    print('PDF',len(d.pages),'pages; protected',len(baseline['protected']))

if __name__=='__main__': main()
