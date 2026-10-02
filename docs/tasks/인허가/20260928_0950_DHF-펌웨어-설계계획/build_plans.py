"""Adapt six retained DOCX packages without rebuilding their formatting."""
from __future__ import annotations
import argparse
import hashlib
import json
import re
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from lxml import etree as E

ROOT = Path(__file__).resolve().parents[4]
TASK = Path(__file__).resolve().parent
SOURCE = Path(r'G:\공유 드라이브\20_프로젝트별\01_project sound1\04_docs\99_참고용DHF\01_설계 계획\SW\펌웨어')
DEST = ROOT/'docs/참고/인허가/사운드1/DHF/01_설계 계획/SW/펌웨어'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
R='http://schemas.openxmlformats.org/officeDocument/2006/relationships'
NS={'w':W,'r':R}

def sha(data): return hashlib.sha256(data).hexdigest()
def text(p): return ''.join(p.xpath('.//w:t/text()',namespaces=NS))
def xml(data): return E.fromstring(data)
def serialize(root): return E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)

def replace(p, old, new):
    """Replace spans across runs, preserving every original run property."""
    if not old: raise ValueError('empty replacement pattern')
    for start in reversed([m.start() for m in re.finditer(re.escape(old),text(p))]):
        nodes=p.xpath('.//w:t',namespaces=NS)
        full=''.join(n.text or '' for n in nodes)
        end=start+len(old); pos=0; inserted=False
        for n in nodes:
            value=n.text or ''; stop=pos+len(value)
            if pos<end and stop>start:
                left=value[:max(0,start-pos)]
                right=value[max(0,end-pos):] if stop>end else ''
                n.text=left+(new if not inserted else '')+right
                n.set('{http://www.w3.org/XML/1998/namespace}space','preserve')
                inserted=True
            pos=stop

def inventory(path):
    with ZipFile(path) as z:
        x=xml(z.read('word/document.xml'))
        return {'name':path.name,'sha256':sha(path.read_bytes()),
            'parts':{n:sha(z.read(n)) for n in z.namelist()},
            'sections':[dict((E.QName(c).localname,dict(c.attrib)) for c in s if E.QName(c).localname in ('pgSz','pgMar','cols','docGrid')) for s in x.xpath('//w:sectPr',namespaces=NS)],
            'paragraphs':len(x.xpath('//w:p',namespaces=NS)), 'tables':len(x.xpath('//w:tbl',namespaces=NS)),
            'fields':x.xpath('//w:instrText/text()',namespaces=NS),
            'drawings':[(i,len(p.xpath('.//w:drawing|.//w:pict',namespaces=NS))) for i,p in enumerate(x.xpath('//w:p',namespaces=NS)) if p.xpath('.//w:drawing|.//w:pict',namespaces=NS)]}

def build(overwrite=False):
    edits=json.loads((TASK/'edits.json').read_text(encoding='utf-8'))
    baseline=json.loads((TASK/'baseline.json').read_text(encoding='utf-8'))
    DEST.mkdir(parents=True,exist_ok=True)
    report=[]
    for path in sorted(SOURCE.glob('*.docx')):
        code=path.name.split('-')[0]
        before=next(b for b in baseline if b['name']==path.name)
        assert sha(path.read_bytes())==before['sha256'], 'source changed'
        target=DEST/path.name.replace('TD20B','TD24A')
        if target.exists() and not overwrite: raise FileExistsError(target)
        with ZipFile(path) as zin:
            parts={n:zin.read(n) for n in zin.namelist()}
            original=dict(parts)
            changes=[];removed_images=[]
            for name in list(parts):
                if not (name=='word/document.xml' or re.match(r'word/(header|footer)\d+\.xml$',name)):
                    continue
                x=xml(parts[name]);paras=x.xpath('//w:p',namespaces=NS)
                for i,p in enumerate(paras):
                    old=text(p)
                    if name=='word/document.xml' and str(i) in edits[code]:
                        where,new,reason,expected=edits[code][str(i)]
                        assert old==expected,(code,i,old,expected)
                        replace(p,old,new)
                        changes.append({'location':where,'part':name,'paragraph':i,'before':old,'after':new,'reason':reason})
                        continue
                    replacements=[('TD20B-A-EXT','TD24A-A-EXT'),('TD20B','TD24A'),('SCMP-FW-TD20','SCMP-FW-TD24A')]
                    # Exact short header typo correction; do not double-expand TD24A.
                    for a,b in replacements:
                        if a in text(p): replace(p,a,b)
                    if name=='word/document.xml' and i<110:
                        if text(p).strip()=='임정우':
                            replace(p,text(p),'김은수')
                        elif text(p).strip()=='사업부장':
                            replace(p,text(p),'수석연구원')
                        elif text(p).strip() in ('이호승','기술이사') or re.fullmatch(r'2020\.\d{2}\.\d{2}',text(p).strip()):
                            replace(p,text(p),'')
                    if 'header' in name:
                        if re.fullmatch(r'2020\.\d{2}\.\d{2}',text(p).strip()): replace(p,text(p),'')
                    if name=='word/document.xml' and i<100:
                        for drawing in p.xpath('.//w:drawing|.//w:pict',namespaces=NS):
                            removed_images.extend(drawing.xpath('.//@r:embed|.//@r:id',namespaces=NS))
                            drawing.getparent().remove(drawing)
                    if text(p)!=old:
                        changes.append({'location':'표지·개정 이력·머리말 또는 공통 제품·문서 식별','part':name,'paragraph':i,'before':old,'after':text(p),'reason':'Sound1 식별 전환 및 사용자 지정 작성자 반영; 날짜·검토·승인 공란'})
                parts[name]=serialize(x)
            # Remove old signature media and only their unused relationships.
            relname='word/_rels/document.xml.rels';rels=xml(parts[relname])
            for rel in list(rels):
                if rel.get('Id') in removed_images:
                    media='word/'+rel.get('Target')
                    if not any(r.get('Target')==rel.get('Target') and r.get('Id') not in removed_images for r in rels):
                        parts.pop(media,None)
                    rels.remove(rel)
            parts[relname]=serialize(rels)
            settings=xml(parts['word/settings.xml']);up=settings.find(f'{{{W}}}updateFields')
            if up is None: up=E.SubElement(settings,f'{{{W}}}updateFields')
            up.set(f'{{{W}}}val','true');parts['word/settings.xml']=serialize(settings)
            # Do not carry the previous author's electronic metadata as current approval.
            if 'docProps/core.xml' in parts:
                core=xml(parts['docProps/core.xml'])
                for n in core:
                    if E.QName(n).localname in ('creator','lastModifiedBy'): n.text=''
                    elif E.QName(n).localname in ('created','modified'): core.remove(n)
                parts['docProps/core.xml']=serialize(core)
            # Preservation gates: source body geometry, styles, numbering and table properties.
            ox=xml(original['word/document.xml']);nx=xml(parts['word/document.xml'])
            for query in ('//w:sectPr','//w:pPr','//w:rPr','//w:tblPr','//w:tblGrid','//w:trPr','//w:tcPr'):
                assert [E.tostring(n) for n in ox.xpath(query,namespaces=NS)]==[E.tostring(n) for n in nx.xpath(query,namespaces=NS)],(code,query)
            for name in ('word/styles.xml','word/numbering.xml','word/fontTable.xml','word/theme/theme1.xml'):
                if name in original: assert parts[name]==original[name]
            with ZipFile(target,'w',ZIP_DEFLATED) as zout:
                for info in zin.infolist():
                    if info.filename in parts: zout.writestr(info,parts[info.filename])
            assert sha(path.read_bytes())==before['sha256']
            report.append({'source':path.name,'target':target.name,'source_sha256':before['sha256'],'target_sha256':sha(target.read_bytes()),
                'changed_parts':[n for n in parts if parts[n]!=original[n]],'removed_parts':[n for n in original if n not in parts],
                'changes':changes,'preservation':'section/paragraph/run/table properties, styles, numbering, fonts, themes preserved'})
            print(code,len(changes),'changes; saved',target.name,flush=True)
    (TASK/'build_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--inspect',action='store_true');ap.add_argument('--overwrite',action='store_true');args=ap.parse_args()
    if args.inspect:
        items=[inventory(p) for p in sorted(SOURCE.glob('*.docx'))]
        (TASK/'baseline.json').write_text(json.dumps(items,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps([{k:v for k,v in b.items() if k not in ('parts','fields')} for b in items],ensure_ascii=False))
    else: build(args.overwrite)
