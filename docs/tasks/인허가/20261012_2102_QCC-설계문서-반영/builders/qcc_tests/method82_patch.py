import json,zipfile,hashlib,os
from pathlib import Path
from lxml import etree as ET
from preserve_package import values
HERE=Path(__file__).resolve().parent;TASK=HERE.parent.parent
N={'x':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'};NS=N['x']
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
with zipfile.ZipFile(HERE/'method82-artifact.xlsx')as z:
 v=values(z,'xl/worksheets/sheet1.xml')['D282']
assert v=='\nD\n'
evidence=TASK/'evidence/qcc_tests_authored.json';ev=json.loads(evidence.read_text(encoding='utf-8'))
patches=[]
for f in ev['files']:
 if f['key']not in['SSTC','SSTR']:continue
 p=Path(f['path']);before=sha(p)
 with zipfile.ZipFile(p)as z:infos=z.infolist();data={i.filename:z.read(i.filename)for i in infos}
 sheet=ET.fromstring(data['xl/worksheets/sheet2.xml']);c=sheet.find("x:sheetData/x:row[@r='282']/x:c[@r='D282']",N)
 assert c is not None
 beforecell=ET.tostring(c)
 for ch in list(c):c.remove(ch)
 c.set('t','inlineStr');isnode=ET.SubElement(c,'{'+NS+'}is');t=ET.SubElement(isnode,'{'+NS+'}t');t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=v
 data['xl/worksheets/sheet2.xml']=ET.tostring(sheet,xml_declaration=True,encoding='UTF-8',standalone=True)
 temp=p.with_name(p.name+'.sys82method')
 with zipfile.ZipFile(temp,'w')as z:
  for i in infos:z.writestr(i,data[i.filename])
 os.replace(temp,p);f['sha256_after']=sha(p)
 patches.append({'key':f['key'],'sheet':f['sheet'],'cell':'D282','merge':'D282:D283','before_sha256':before,'after_sha256':sha(p),'value':'D','reason':'Functional demonstration; no quantitative tolerance is provided. 1000Hz is only a stimulus input.','preserved_package_members_except_main_sheet':True})
 # Check every package member outside main sheet is byte-identical to the input.
 with zipfile.ZipFile(p)as z:
  assert all(z.read(i.filename)==data[i.filename]for i in infos)
ev.setdefault('targeted_patches',[]).extend(patches)
ev['aux_QA']={'history_sheet':'시험 보조자료','scope':'A2 plus B/C/D at80,83,184,185,190,199..202,206..214,222..224,227..229 mirror current main criteria/results; history retained.','SYS82_method':'Only main D282:D283 contains a method; auxiliary sheet has no method column or added SYS82 condition.','cybersecurity_sheet':'사이버보안 시험 연계 A:J exact criterion/report common values; unchanged by method patch.'}
evidence.write_text(json.dumps(ev,ensure_ascii=False,indent=2),encoding='utf-8')
# Keep task-local intended payload consistent without replaying the full builder.
pp=HERE/'payload.json';payload=json.loads(pp.read_text(encoding='utf-8'))
for b in payload['books']:
 if b['key']in['SSTC','SSTR']:b['cells']['D282']=v
pp.write_text(json.dumps(payload,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(patches,ensure_ascii=True))
