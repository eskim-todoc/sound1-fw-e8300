from pathlib import Path
from lxml import etree as E
import json,zipfile,os,copy,sys
from integrate import N,texts,sha,sheetmap,vals
H=Path(__file__).resolve().parent
P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
QA=json.loads((H/'qa.json').read_text(encoding='utf-8'))
expected={x['key']:x['sha256'] for x in QA['files']}
out=[];do_write='--write' in sys.argv
for key,b in P['books'].items():
 p=Path(b['path']);before=sha(p)
 assert before==expected[key],(key,'concurrent change',before,expected[key])
 with zipfile.ZipFile(p) as z:infos=z.infolist();data={i.filename:z.read(i.filename) for i in infos}
 original=data.copy();changes=[];removed=0
 for sn,member in sheetmap(data).items():
  root=E.fromstring(data[member]);before_vals=vals(data,member);cells=[]
  for c in root.findall('.//x:sheetData/x:row/x:c',N):
   isn=c.find('x:is',N)
   if isn is None:continue
   old_text=texts(isn);nonempty=[E.tostring(r) for r in isn.findall('x:r',N) if texts(r)]
   empties=[r for r in isn.findall('x:r',N) if not texts(r)]
   if not empties:continue
   assert all(r.find('x:t',N) is not None for r in empties),(key,sn,c.get('r'))
   for r in empties:isn.remove(r)
   assert texts(isn)==old_text
   assert [E.tostring(r) for r in isn.findall('x:r',N)]==nonempty
   cells.append({'cell':c.get('r'),'empty_runs_removed':len(empties),'text_preserved':True,'all_nonempty_run_properties_preserved':True})
   removed+=len(empties)
  if cells:
   data[member]=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
   assert vals(data,member)==before_vals
   changes.append({'sheet':sn,'member':member,'cells':cells})
 target=p if do_write else H/(key+'-empty-run-repair.xlsx')
 if do_write:
  back=H/'native-fix-before';back.mkdir(exist_ok=True)
  (back/(key+'.xlsx')).write_bytes(p.read_bytes())
 temp=target.with_name(target.name+'.repairing')
 with zipfile.ZipFile(temp,'w') as z:
  for i in infos:z.writestr(i,data[i.filename])
 assert sha(p)==before,(key,'changed while repairing')
 os.replace(temp,target)
 changed_members={x['member'] for x in changes}
 assert all(data[m]==original[m] for m in data if m not in changed_members)
 with zipfile.ZipFile(target) as z:assert z.testzip() is None
 out.append({'key':key,'source_path':str(p),'path':str(target),'sha256_before':before,'sha256_after':sha(target),'empty_runs_removed':removed,'changes':changes,'other_package_members_byte_identical':True,'all_cell_values_preserved':True,'all_nonempty_rich_runs_byte_identical':True})
(H/('empty-run-repair-applied.json' if do_write else 'empty-run-repair-candidates.json')).write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'write':do_write,'files':[{k:v for k,v in x.items() if k!='changes'} for x in out]},ensure_ascii=True))
