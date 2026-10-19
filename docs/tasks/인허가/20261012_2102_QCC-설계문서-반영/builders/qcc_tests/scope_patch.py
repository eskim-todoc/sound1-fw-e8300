import json,zipfile,hashlib,os
from pathlib import Path
from lxml import etree as ET
from preserve_package import values
HERE=Path(__file__).resolve().parent;TASK=HERE.parent.parent;NS='http://schemas.openxmlformats.org/spreadsheetml/2006/main';N={'x':NS}
payload=json.loads((HERE/'scope-patch.json').read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
with zipfile.ZipFile(HERE/'scope-artifact.xlsx')as z:authored=values(z,'xl/worksheets/sheet1.xml')
assert all(authored[c]==v for c,v in payload['cells'].items())
evp=TASK/'evidence/qcc_tests_authored.json';ev=json.loads(evp.read_text(encoding='utf-8'));patches=[]
for f in ev['files']:
 if f['key']not in['SUVC','SUVR']:continue
 path=Path(f['path']);before=sha(path)
 with zipfile.ZipFile(path)as z:infos=z.infolist();data={i.filename:z.read(i.filename)for i in infos}
 beforedata=data.copy();s=ET.fromstring(data['xl/worksheets/sheet2.xml'])
 for addr,value in payload['cells'].items():
  c=s.find(f".//x:sheetData/x:row/x:c[@r='{addr}']",N);assert c is not None
  for child in list(c):c.remove(child)
  c.set('t','inlineStr');isnode=ET.SubElement(c,'{'+NS+'}is');t=ET.SubElement(isnode,'{'+NS+'}t');t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=authored[addr]
 breaks=s.find('x:rowBreaks',N)
 if breaks is None:
  breaks=ET.Element('{'+NS+'}rowBreaks')
  later=next((e for e in s if ET.QName(e).localname in['colBreaks','customProperties','cellWatches','ignoredErrors','smartTags','drawing','legacyDrawing','legacyDrawingHF','picture','oleObjects','controls','webPublishItems','tableParts','extLst']),None)
  if later is None:s.append(breaks)
  else:s.insert(s.index(later),breaks)
 ids={int(b.get('id'))for b in breaks}
 for row in payload['break_before_rows']:
  if row-1 not in ids:ET.SubElement(breaks,'{'+NS+'}brk',id=str(row-1),min='0',max='16383',man='1')
 for child in sorted(list(breaks),key=lambda e:int(e.get('id'))):breaks.remove(child);breaks.append(child)
 breaks.set('count',str(len(breaks)));breaks.set('manualBreakCount',str(sum(b.get('man')=='1'for b in breaks)))
 data['xl/worksheets/sheet2.xml']=ET.tostring(s,xml_declaration=True,encoding='UTF-8',standalone=True)
 temp=path.with_name(path.name+'.qccscopes')
 with zipfile.ZipFile(temp,'w')as z:
  for i in infos:z.writestr(i,data[i.filename])
 os.replace(temp,path);f['sha256_after']=sha(path)
 assert all(beforedata[n]==data[n]for n in data if n!='xl/worksheets/sheet2.xml')
 patches.append({'key':f['key'],'sheet':f['sheet'],'cells':list(payload['cells']),'break_before_rows':payload['break_before_rows'],'row_break_ids':[r-1 for r in payload['break_before_rows']],'hash_before':before,'hash_after':sha(path),'scope':'QCC state/input support vs E8300 identity/RF/stimulation/alarm complete verification; original evidence scope preserved.','layout':'15 group-start manual page breaks; rows/heights/merges unchanged.'})
ev.setdefault('targeted_patches',[]).extend(patches);evp.write_text(json.dumps(ev,ensure_ascii=False,indent=2),encoding='utf-8')
pp=HERE/'payload.json';p=json.loads(pp.read_text(encoding='utf-8'))
for b in p['books']:
 if b['key']in['SUVC','SUVR']:b['cells'].update(payload['cells'])
pp.write_text(json.dumps(p,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(patches,ensure_ascii=True))
