import json,zipfile,hashlib,os
from pathlib import Path
from lxml import etree as E
from preserve_package import values,Styles,put
HERE=Path(__file__).resolve().parent;TASK=HERE.parent.parent;NS='http://schemas.openxmlformats.org/spreadsheetml/2006/main';N={'x':NS}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
p=json.loads((HERE/'spi-patch.json').read_text(encoding='utf-8'));e=TASK/'evidence/qcc_tests_authored.json';ev=json.loads(e.read_text(encoding='utf-8'));patches=[]
with zipfile.ZipFile(HERE/'spi-artifact.xlsx')as z:au={'unit':values(z,'xl/worksheets/sheet1.xml'),'integration':values(z,'xl/worksheets/sheet2.xml')}
for kind in['unit','integration']:assert all(au[kind][c]==v for c,v in p[kind].items())
for f in ev['files']:
 if f['key']not in['SUVC','SUVR','SITC','SITR']:continue
 kind='unit'if f['key']in['SUVC','SUVR']else'integration';path=Path(f['path']);before=sha(path)
 with zipfile.ZipFile(path)as z:infos=z.infolist();data={i.filename:z.read(i.filename)for i in infos}
 old=data.copy();s=E.fromstring(data['xl/worksheets/sheet2.xml']);st=Styles(E.fromstring(data['xl/styles.xml']))
 for addr,value in p[kind].items():
  c=s.find(f".//x:sheetData/x:row/x:c[@r='{addr}']",N);assert c is not None
  put(c,au[kind][addr],st)
 for row,height in p[kind+'_heights'].items():
  r=s.find(f"x:sheetData/x:row[@r='{row}']",N);r.set('ht',str(height));r.set('customHeight','1')
 if kind=='integration':
  br=s.find('x:rowBreaks',N)
  if br is None:
   br=E.Element('{'+NS+'}rowBreaks');later=next((x for x in s if E.QName(x).localname in['colBreaks','customProperties','cellWatches','ignoredErrors','smartTags','drawing','legacyDrawing','legacyDrawingHF','picture','oleObjects','controls','webPublishItems','tableParts','extLst']),None)
   if later is None:s.append(br)
   else:s.insert(s.index(later),br)
  ids={int(c.get('id'))for c in br}
  for row in p['integration_break_before_rows']:
   if row-1 not in ids:E.SubElement(br,'{'+NS+'}brk',id=str(row-1),min='0',max='16383',man='1')
  for c in sorted(list(br),key=lambda c:int(c.get('id'))):br.remove(c);br.append(c)
  br.set('count',str(len(br)));br.set('manualBreakCount',str(sum(c.get('man')=='1'for c in br)))
 data['xl/worksheets/sheet2.xml']=E.tostring(s,xml_declaration=True,encoding='UTF-8',standalone=True)
 assert all(old[n]==data[n]for n in data if n!='xl/worksheets/sheet2.xml')
 temp=path.with_name(path.name+'.qccspi21')
 with zipfile.ZipFile(temp,'w')as z:
  for i in infos:z.writestr(i,data[i.filename])
 os.replace(temp,path);f['sha256_after']=sha(path)
 patches.append({'key':f['key'],'sheet':f['sheet'],'cells':list(p[kind]),'height_rows':p[kind+'_heights'],'manual_break_before_rows':p['integration_break_before_rows']if kind=='integration'else'unchanged15unitgroupbreaks','hash_before':before,'hash_after':sha(path),'scope':'logical type+payload<=20byte vs physicalSPI20+separate length1=21byte; QCC actual length mapping remains pending. integration019 input/state support separated from full E8300RFidentity/stimulation verdicts.'})
ev.setdefault('targeted_patches',[]).extend(patches)
ev['format_diagnosis']['native_open_probe']=json.loads((TASK/'evidence/qcc_tests_native_open.json').read_text(encoding='utf-8-sig'))
e.write_text(json.dumps(ev,ensure_ascii=False,indent=2),encoding='utf-8')
payloadfile=HERE/'payload.json';full=json.loads(payloadfile.read_text(encoding='utf-8'))
for b in full['books']:
 if b['key']in['SUVC','SUVR','SITC','SITR']:
  kind='unit'if b['key']in['SUVC','SUVR']else'integration';b['cells'].update(p[kind]);b['heights'].update(p[kind+'_heights'])
payloadfile.write_text(json.dumps(full,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(patches,ensure_ascii=True))
