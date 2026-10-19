import json,zipfile,hashlib,os
from pathlib import Path
from lxml import etree as E
HERE=Path(__file__).resolve().parent;TASK=HERE.parent.parent
N={'x':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
evp=TASK/'evidence/qcc_tests_authored.json';ev=json.loads(evp.read_text(encoding='utf-8'));f=next(f for f in ev['files']if f['key']=='SSTR');p=Path(f['path']);before=sha(p)
backup=TASK/'qa/qcc_tests/SSTR-before-order-repair.xlsx'
backup.write_bytes(p.read_bytes())
with zipfile.ZipFile(p)as z:infos=z.infolist();data={i.filename:z.read(i.filename)for i in infos}
old=data.copy();s=E.fromstring(data['xl/worksheets/sheet3.xml']);merge=s.find('x:mergeCells',N);phon=s.find('x:phoneticPr',N)
beforeorder=[E.QName(e).localname for e in s]
assert merge is not None and phon is not None
assert s.index(merge)>s.index(phon),'This targeted repair expects the diagnosed invalid order'
oldcells=E.tostring(s.find('x:sheetData',N));oldmerge=E.tostring(merge)
s.remove(merge);s.insert(s.index(phon),merge)
assert E.tostring(s.find('x:sheetData',N))==oldcells and E.tostring(merge)==oldmerge
afterorder=[E.QName(e).localname for e in s]
data['xl/worksheets/sheet3.xml']=E.tostring(s,xml_declaration=True,encoding='UTF-8',standalone=True)
temp=p.with_name(p.name+'.schemaorder')
with zipfile.ZipFile(temp,'w')as z:
 for i in infos:z.writestr(i,data[i.filename])
os.replace(temp,p)
assert all(old[n]==data[n]for n in old if n!='xl/worksheets/sheet3.xml')
patch={'key':'SSTR','part':'xl/worksheets/sheet3.xml','sheet':'시험 보조자료','change':'move mergeCells before phoneticPr; preserve sheetData and A2:D2 merger exactly','before_order':beforeorder,'after_order':afterorder,'before_sha256':before,'after_sha256':sha(p),'schema_support':'https://learn.microsoft.com/en-us/office/open-xml/spreadsheet/how-to-merge-two-adjacent-cells-in-a-spreadsheet','backup':str(backup)}
f['sha256_after']=sha(p);ev.setdefault('targeted_patches',[]).append(patch)
ev['format_diagnosis']=patch
evp.write_text(json.dumps(ev,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(patch,ensure_ascii=True))
