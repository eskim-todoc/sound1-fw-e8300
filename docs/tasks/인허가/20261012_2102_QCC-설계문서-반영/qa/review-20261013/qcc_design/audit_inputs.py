from pathlib import Path
from copy import deepcopy
import hashlib, json, re, shutil, zipfile
from lxml import etree as ET

ROOT = Path(__file__).resolve().parent
TASK = ROOT.parents[2]
BASE = Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
PROJECT = Path('E:/workspace/projects/sound1-fw-e8300-backup')
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
NS = {'w':W, 'a':'http://schemas.openxmlformats.org/drawingml/2006/main', 'r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
q=lambda n:'{'+W+'}'+n

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def accepted(el):
    x=deepcopy(el)
    for row in x.xpath('.//w:tr[w:trPr/w:del]',namespaces=NS):row.getparent().remove(row)
    for d in x.xpath('.//w:del',namespaces=NS):
        if d.getparent() is not None:d.getparent().remove(d)
    return x

def txt(x):
    return ''.join(('\n' if e.tag==q('br') else '\t' if e.tag==q('tab') else e.text or '') for e in x.iter() if e.tag in [q('t'),q('br'),q('tab')])

def inspect(p,code,role):
    dest=ROOT/'before'/p.name
    dest.parent.mkdir(parents=True,exist_ok=True)
    if not dest.exists():shutil.copy2(p,dest)
    elif sha(dest)!=sha(p):raise RuntimeError('existing baseline differs: '+str(p))
    with zipfile.ZipFile(p) as z:
        root=ET.fromstring(z.read('word/document.xml'))
        doc=accepted(root)
        body=doc.find('w:body',NS)
        sty=ET.fromstring(z.read('word/styles.xml'))
        style_names={x.get(q('styleId')):x.find('w:name',NS).get(q('val')) for x in sty if x.find('w:name',NS) is not None}
        paragraphs=[]; tables=[]; lines=[]
        for n,b in enumerate(body,1):
            if b.tag==q('p'):
                t=txt(b); st=b.find('w:pPr/w:pStyle',NS)
                ps=st.get(q('val')) if st is not None else ''
                paragraphs.append({'block':n,'style':ps,'style_name':style_names.get(ps,ps),'text':t})
                lines.append(f'[{n} P {ps}] {t}')
            elif b.tag==q('tbl'):
                rows=[[txt(c) for c in r.findall('w:tc',NS)] for r in b.findall('w:tr',NS)]
                pr=b.find('w:tblPr',NS)
                grid=b.find('w:tblGrid',NS)
                info={'block':n,'rows':rows,'grid':[c.get(q('w')) for c in grid] if grid is not None else [],'properties':ET.tostring(pr,encoding='unicode') if pr is not None else ''}
                tables.append(info)
                lines.append(f'[{n} TABLE {len(rows)} rows]')
                lines.extend(' | '.join(r) for r in rows)
            elif b.tag==q('sdt'):lines.append(f'[{n} SDT excluded TOC]')
        current=ROOT/'current'
        current.mkdir(exist_ok=True)
        (current/f'{code}-{role}.txt').write_text('\n'.join(lines),encoding='utf-8')
        with zipfile.ZipFile(p) as zz:
            rels=ET.fromstring(zz.read('word/_rels/document.xml.rels'))
            rm={r.get('Id'):r.get('Target') for r in rels}
            images=[]
            for i,blip in enumerate(doc.xpath('.//a:blip[@r:embed]',namespaces=NS),1):
                rid=blip.get('{'+NS['r']+'}embed'); target=rm.get(rid,'')
                part='word/'+target
                if part in zz.namelist():
                    data=zz.read(part)
                    img=current/f'{code}-{role}-image-{i:02d}{Path(part).suffix}'
                    img.write_bytes(data)
                    images.append({'sequence':i,'rid':rid,'part':part,'sha256':hashlib.sha256(data).hexdigest(),'extracted':str(img)})
        return {'code':code,'role':role,'path':str(p),'sha256':sha(p),'baseline':str(dest),'blocks':len(body),'paragraphs':paragraphs,'tables':tables,'images':images,'tdc_styles':{k:v for k,v in style_names.items() if v.startswith('TDC')}}

def main():
    records=[]
    for code in ['SAD','SDDD','SIP']:
        for role,prefix in [('E','[E8300] '),('current','[E8300-QCC] '),('Q','QCC_')]:
            directory=BASE/'서석일_작성' if role=='Q' else BASE
            paths=[p for p in directory.iterdir() if p.is_file() and p.name.startswith(prefix+code+'-') and p.suffix=='.docx']
            if len(paths)!=1:raise RuntimeError((code,role,paths))
            records.append(inspect(paths[0],code,role))
    image_dir=PROJECT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/이미지 참조'
    for name in ['시스템 블록도.png','소프트웨어 구조도.png']:
        p=image_dir/name; d=ROOT/'before'/name
        if not d.exists():shutil.copy2(p,d)
        elif sha(p)!=sha(d):raise RuntimeError('local image changed before snapshot')
    source=TASK/'qcc_design_figures.py'
    shutil.copy2(source,ROOT/'before'/source.name)
    dotx=Path('E:/workspace/rules-for-codex/지침/양식/Word/TDC 공통 스타일.dotx')
    with zipfile.ZipFile(dotx) as z:
        styles=ET.fromstring(z.read('word/styles.xml'))
        relevant=[ET.tostring(s,encoding='unicode') for s in styles if s.find('w:name',NS) is not None and s.find('w:name',NS).get(q('val'),'').startswith('TDC')]
        for part in ['word/styles.xml','word/numbering.xml','word/settings.xml','word/document.xml']:ET.fromstring(z.read(part))
    (ROOT/'input-audit.json').write_text(json.dumps({'documents':records,'dotx_sha256':sha(dotx),'dotx_tdc_style_xml':relevant},ensure_ascii=False,indent=2),encoding='utf-8')
    summary=[]
    for r in records:
        if r['role']=='current':
            bad=[{'block':p['block'],'text':p['text']} for p in r['paragraphs'] if re.search('제공|원문|소속 유닛|마이크|DMIC',p['text'])]
            qt=[{'block':t['block'],'grid':t['grid'],'first_rows':t['rows'][:2]} for t in r['tables'] if any('SU-ID-QCC' in c or 'SI-ID-QCC' in c for row in t['rows'] for c in row)]
            summary.append({'code':r['code'],'sha256':r['sha256'],'blocks':r['blocks'],'tdc_style_count':len(r['tdc_styles']),'paragraph_candidate_count':len(bad),'paragraph_candidates':bad[:8],'qcc_table_count':len(qt),'qcc_tables':qt[:4]})
    print(json.dumps(summary,ensure_ascii=True))

if __name__=='__main__':main()
