import hashlib,json,pathlib,re,zipfile
from lxml import etree as E
ROOT=pathlib.Path(r'E:\workspace\projects\sound1-fw-e8300-backup\docs\tasks\인허가\20261012_2102_QCC-설계문서-반영')
DEST=pathlib.Path(r'G:\공유 드라이브\20_프로젝트별\01_project sound1\08_DHF\02_설계 입력\SW\외부기')
X='http://schemas.openxmlformats.org/spreadsheetml/2006/main';tag=lambda t:'{'+X+'}'+t
evfile=ROOT/'evidence/qcc_requirements_authored.json';ev=json.loads(evfile.read_text(encoding='utf8'))
out=pathlib.Path(ev['outputs'][0]['file']);oldhash=ev['outputs'][0]['sha256']
assert hashlib.sha256(out.read_bytes()).hexdigest().upper()==oldhash
source=next(DEST.glob('[[]E8300] SRS-*.xlsx'))
with zipfile.ZipFile(source) as z:oldchain=E.fromstring(z.read('xl/calcChain.xml'))
with zipfile.ZipFile(out) as z:parts={n:z.read(n) for n in z.namelist()}
sid=E.fromstring(parts['xl/workbook.xml']).find(tag('sheets'))[1].get('sheetId');current=None
for c in oldchain:
 current=c.get('i',current)
 if current==sid:
  ref=c.get('r');num=int(re.search(r'\d+$',ref)[0])
  if num>=49:c.set('r',re.sub(r'\d+$',str(num+1),ref))
E.SubElement(oldchain,tag('c'),r='G49',i=sid);E.SubElement(oldchain,tag('c'),r='G80',i=sid)
parts['xl/calcChain.xml']=E.tostring(oldchain,encoding='UTF-8',xml_declaration=True,standalone=True)
assert hashlib.sha256(out.read_bytes()).hexdigest().upper()==oldhash
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 for n,v in parts.items():z.writestr(n,v)
newhash=hashlib.sha256(out.read_bytes()).hexdigest().upper();ev['outputs'][0]['sha256']=newhash
ev['outputs'][0]['calc_chain_correction']={'previous_hash':oldhash,'actual_body_sheet_id':sid,'shifted_source_formula_references':True,'added':['G49','G80']}
evfile.write_text(json.dumps(ev,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'saved':str(out),'sha256':newhash,'actual_sheet_id':sid},ensure_ascii=False))
