"""Artifact Tool 작성 결과를 원본 기준서의 정확한 서식/행에 결합한다.

기준서 공통 셀과 기존 보고서 표지/보조 시트를 재작성하지 않는다.
지원 도구의 스타일/공유 문자열 재직렬화로 원본이 달라지는 부분을 보존하고,
새 결과 열/보조 시트/그림은 Artifact Tool에서 작성한 셀과 객체만 가져온다.
"""
import copy
import json
import posixpath
import zipfile
from pathlib import Path
from lxml import etree as E

TASK=Path(__file__).resolve().parent
QA=TASK/'qa/criterion-report-alignment'
NS='http://schemas.openxmlformats.org/spreadsheetml/2006/main'
RNS='http://schemas.openxmlformats.org/officeDocument/2006/relationships'
PNS='http://schemas.openxmlformats.org/package/2006/relationships'
CNS='http://schemas.openxmlformats.org/package/2006/content-types'
N={'s':NS,'r':RNS,'p':PNS}

def parse(data): return E.fromstring(data)
def serialize(node): return E.tostring(node,xml_declaration=True,encoding='UTF-8',standalone=True)
def parts(path):
    with zipfile.ZipFile(path) as z: return {n:z.read(n) for n in z.namelist()}
def relpath(path): return posixpath.join(posixpath.dirname(path),'_rels',posixpath.basename(path)+'.rels')
def resolve(path,target): return posixpath.normpath(posixpath.join(posixpath.dirname(path),target)).lstrip('/')
def col(address):
    n=0
    for c in address:
        if c.isalpha(): n=n*26+ord(c)-64
        else: break
    return n
def sheet_path(package,name):
    wb=parse(package['xl/workbook.xml']); rel=parse(package['xl/_rels/workbook.xml.rels'])
    rid=next(s.get('{'+RNS+'}id') for s in wb.find('s:sheets',N) if s.get('name')==name)
    return resolve('xl/workbook.xml',next(r.get('Target') for r in rel if r.get('Id')==rid))

class Importer:
    def __init__(self,target,source,prefix):
        self.target=target;self.source=source;self.prefix=prefix
        self.styles=parse(target['xl/styles.xml']);self.srcstyles=parse(source['xl/styles.xml'])
        self.sst=parse(source['xl/sharedStrings.xml']) if 'xl/sharedStrings.xml' in source else None
        self.maps={};self.copied={};self.content=parse(target['[Content_Types].xml']);self.srccontent=parse(source['[Content_Types].xml'])
        self.init_styles()

    def init_styles(self):
        for tag in ['fonts','fills','borders','numFmts','cellStyleXfs','cellXfs','dxfs']:
            src=self.srcstyles.find('s:'+tag,N)
            if src is None: continue
            dst=self.styles.find('s:'+tag,N)
            if dst is None:
                dst=E.Element('{'+NS+'}'+tag)
                # Insert in the schema's resource order.
                order=['numFmts','fonts','fills','borders','cellStyleXfs','cellXfs','cellStyles','dxfs','tableStyles','colors','extLst']
                position=next((i for i,child in enumerate(self.styles) if order.index(E.QName(child).localname)>order.index(tag)),len(self.styles))
                self.styles.insert(position,dst)
            mapping={}
            for index,element in enumerate(src):
                item=copy.deepcopy(element)
                if tag in ['cellStyleXfs','cellXfs']:
                    for attr,resource in [('fontId','fonts'),('fillId','fills'),('borderId','borders'),('xfId','cellStyleXfs')]:
                        if attr in item.attrib:item.set(attr,str(self.maps.get(resource,{}).get(int(item.get(attr)),int(item.get(attr)))))
                    old=int(item.get('numFmtId','0'));item.set('numFmtId',str(self.maps.get('numFmts',{}).get(old,old)))
                if tag=='numFmts':
                    old=int(item.get('numFmtId'));existing=next((v for v in dst if v.get('formatCode')==item.get('formatCode')),None)
                    if existing is None:
                        number=max([163]+[int(v.get('numFmtId')) for v in dst])+1;item.set('numFmtId',str(number));dst.append(item)
                    else:number=int(existing.get('numFmtId'))
                    mapping[old]=number
                else:
                    key=E.tostring(item,method='c14n')
                    match=next((i for i,v in enumerate(dst) if E.tostring(v,method='c14n')==key),None)
                    if match is None:match=len(dst);dst.append(item)
                    mapping[index]=match
            dst.set('count',str(len(dst)));self.maps[tag]=mapping

    def convert_sheet(self,node):
        node=copy.deepcopy(node)
        for c in node.findall('.//s:c',N):
            c.set('s',str(self.maps['cellXfs'][int(c.get('s','0'))]))
            if c.get('t')=='s':
                value=c.find('s:v',N)
                if value is not None:
                    si=self.sst[int(value.text)];c.remove(value)
                    ins=E.SubElement(c,'{'+NS+'}is')
                    for child in si:ins.append(copy.deepcopy(child))
                    c.set('t','inlineStr')
        for row in node.findall('.//s:row',N):
            if 's' in row.attrib:row.set('s',str(self.maps['cellXfs'][int(row.get('s'))]))
        for c in node.findall('.//s:col',N):
            if 'style' in c.attrib:c.set('style',str(self.maps['cellXfs'][int(c.get('style'))]))
        for rule in node.findall('.//s:cfRule',N):
            if 'dxfId' in rule.attrib:rule.set('dxfId',str(self.maps['dxfs'][int(rule.get('dxfId'))]))
        return node

    def copy_part(self,old,new=None):
        if old in self.copied:return self.copied[old]
        if new is None:new=posixpath.join(posixpath.dirname(old),self.prefix+'_'+posixpath.basename(old))
        self.copied[old]=new
        raw=self.source[old]
        if old.startswith('xl/worksheets/') and old.endswith('.xml'):raw=serialize(self.convert_sheet(parse(raw)))
        self.target[new]=raw
        original=next((t for t in self.srccontent if t.get('PartName')=='/'+old),None)
        if original is not None:
            ov=copy.deepcopy(original);ov.set('PartName','/'+new)
            if not any(t.get('PartName')=='/'+new for t in self.content):self.content.append(ov)
        extension=old.rsplit('.',1)[-1]
        default=next((t for t in self.srccontent if t.get('Extension')==extension),None)
        if default is not None and not any(t.get('Extension')==extension for t in self.content):self.content.append(copy.deepcopy(default))
        rpath=relpath(old)
        if rpath in self.source:
            rels=parse(self.source[rpath])
            for rel in rels:
                if rel.get('TargetMode')=='External':continue
                dep=resolve(old,rel.get('Target'));mapped=self.copy_part(dep)
                rel.set('Target',posixpath.relpath(mapped,posixpath.dirname(new)))
            self.target[relpath(new)]=serialize(rels)
        return new

    def flush(self):
        self.target['xl/styles.xml']=serialize(self.styles)
        self.target['[Content_Types].xml']=serialize(self.content)

def build(p):
    target=parts(p['report']);criterion=parts(p['criterion']);authored=parts(QA/f"{p['kind']}-authored.xlsx")
    old_main=sheet_path(target,p['reportSheet']);criterion_main=sheet_path(criterion,p['criterionSheet']);authored_main=sheet_path(authored,p['criterionSheet'])
    source_import=Importer(target,criterion,'aligned_'+p['kind']+'_source')
    target.pop(relpath(old_main),None)
    # Preserve the criterion's printer settings and other native worksheet relationships.
    source_import.copy_part(criterion_main,old_main)
    root=source_import.convert_sheet(parse(criterion[criterion_main]));source_import.flush()
    added_import=Importer(target,authored,'aligned_'+p['kind'])
    authored_root=added_import.convert_sheet(parse(authored[authored_main]))
    source_rows=root.find('s:sheetData',N);added_rows=authored_root.find('s:sheetData',N)
    byrow={int(r.get('r')):r for r in source_rows}
    for r in added_rows:
        number=int(r.get('r'))
        assert number in byrow or all(col(c.get('r'))>p['commonEnd'] for c in r)
        dest=byrow.get(number)
        if dest is None:dest=E.SubElement(source_rows,'{'+NS+'}row',r=str(number));byrow[number]=dest
        if number in {t['row'] for t in p['records']}:
            for a in ['ht','customHeight']:
                if a in r.attrib:dest.set(a,r.get(a))
        for c in list(dest):
            if col(c.get('r'))>p['commonEnd']:dest.remove(c)
        for c in r:
            if col(c.get('r'))>p['commonEnd'] or c.get('r')=='B2':
                original=next((x for x in dest if x.get('r')==c.get('r')),None)
                changed=copy.deepcopy(c)
                if c.get('r')=='B2' and original is not None:changed.set('s',original.get('s','0'))
                if original is not None:dest.remove(original)
                dest.append(changed)
        dest[:]=sorted(dest,key=lambda c:col(c.get('r')))
    # Blank rows absent from the source XML must remain in ascending worksheet order.
    source_rows[:]=sorted(source_rows,key=lambda r:int(r.get('r')))
    cols=root.find('s:cols',N)
    for c in list(cols):
        if int(c.get('min'))>p['commonEnd']:cols.remove(c)
        elif int(c.get('max'))>p['commonEnd']:c.set('max',str(p['commonEnd']))
    for c in authored_root.findall('s:cols/s:col',N):
        if int(c.get('max'))>p['commonEnd']:
            cp=copy.deepcopy(c);cp.set('min',str(max(p['commonEnd']+1,int(cp.get('min')))));cols.append(cp)
    merges=root.find('s:mergeCells',N)
    for m in authored_root.findall('s:mergeCells/s:mergeCell',N):
        if col(m.get('ref').split(':')[0])>p['commonEnd']:merges.append(copy.deepcopy(m))
    merges.set('count',str(len(merges)))
    root.find('s:dimension',N).set('ref',f"A1:{chr(64+p['commonEnd']+2)}{len(p['criterionSheets'][p['criterionSheet']]['rows'])}")
    target[old_main]=serialize(root)
    # The criterion contains no main drawings. Old report-only drawing anchors are not reused.
    wb=parse(target['xl/workbook.xml']);rels=parse(target['xl/_rels/workbook.xml.rels'])
    sheets=wb.find('s:sheets',N)
    for name in ['시험 보조자료','예상 그림']:
        src=sheet_path(authored,name)
        new=added_import.copy_part(src)
        rid='rIdAlignment'+p['kind']+str(len(sheets))
        E.SubElement(rels,'{'+PNS+'}Relationship',Id=rid,Type=RNS+'/worksheet',Target=posixpath.relpath(new,'xl'))
        E.SubElement(sheets,'{'+NS+'}sheet',name=name,sheetId=str(max(int(s.get('sheetId')) for s in sheets)+1),attrib={'{'+RNS+'}id':rid})
    names=wb.find('s:definedNames',N)
    if names is None:names=E.SubElement(wb,'{'+NS+'}definedNames')
    index=next(i for i,s in enumerate(sheets) if s.get('name')==p['reportSheet'])
    for dn in list(names):
        if dn.get('localSheetId')==str(index) and dn.get('name') in ['_xlnm.Print_Area','_xlnm.Print_Titles']:names.remove(dn)
    end=len(p['criterionSheets'][p['criterionSheet']]['rows'])
    E.SubElement(names,'{'+NS+'}definedName',name='_xlnm.Print_Area',localSheetId=str(index)).text=f"'{p['reportSheet']}'!$B$2:${chr(64+p['commonEnd']+2)}${end}"
    E.SubElement(names,'{'+NS+'}definedName',name='_xlnm.Print_Titles',localSheetId=str(index)).text=f"'{p['reportSheet']}'!${p['header']}:${p['header']+1}"
    target['xl/workbook.xml']=serialize(wb);target['xl/_rels/workbook.xml.rels']=serialize(rels)
    added_import.flush()
    candidate=QA/f"{p['kind']}-candidate.xlsx"
    with zipfile.ZipFile(candidate,'w',zipfile.ZIP_DEFLATED) as z:
        for name,value in target.items():z.writestr(name,value)
    print(p['kind'],candidate.name,'original common cells/merges retained')

if __name__=='__main__':
    for p in json.loads((QA/'plan.json').read_text(encoding='utf-8')):build(p)
