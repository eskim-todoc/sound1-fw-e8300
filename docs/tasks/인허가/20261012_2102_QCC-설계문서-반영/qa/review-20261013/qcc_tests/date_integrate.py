from pathlib import Path
from lxml import etree as E
import json,zipfile,math,os,sys
from integrate import sheetmap,vals,N,sha,ref,required_height,rich
H=Path(__file__).resolve().parent;F=json.loads((H/'date-plan.json').read_text(encoding='utf-8'))
def package(p):
 with zipfile.ZipFile(p) as z:return z.infolist(),{i.filename:z.read(i.filename) for i in z.infolist()}
pkgs={k:package(v['path']) for k,v in F.items()};records=[];save={};splits=[]
for k,p in F.items():
 infos,data=pkgs[k];orig=data.copy();before=sha(p['path']);assert before==p['hash']
 sn=p['sheet'];member=sheetmap(data)[sn];root=E.fromstring(data[member]);existing=vals(data,member)
 rows={int(x.get('r')):x for x in root.find('x:sheetData',N)};cells={c.get('r'):c for c in root.findall('.//x:sheetData/x:row/x:c',N)}
 styles=E.fromstring(data['xl/styles.xml']);fonts=styles.find('x:fonts',N);xfs=styles.find('x:cellXfs',N)
 sk={'SUVR':'SUVC','SITR':'SITC','SSTR':'SSTC'}.get(k,k);sd=pkgs[sk][1];sr=E.fromstring(sd[sheetmap(sd)[F[sk]['sheet']]])
 sc={c.get('r'):c for c in sr.findall('.//x:sheetData/x:row/x:c',N)};ss=E.fromstring(sd['xl/styles.xml']);sf=ss.find('x:fonts',N);sx=ss.find('x:cellXfs',N)
 with zipfile.ZipFile(H/(k+'-date-authored.xlsx')) as z:stage={i.filename:z.read(i.filename) for i in z.infolist()}
 sv=vals(stage,sheetmap(stage)[sn]);widths={}
 for col in root.findall('x:cols/x:col',N):
  for i in range(int(col.get('min')),int(col.get('max'))+1):widths[i]=float(col.get('width','13'))
 merges=[m.get('ref') for m in root.findall('x:mergeCells/x:mergeCell',N)];heights={}
 for a,c in p['changes'].items():
  assert existing[a]==c['before'] and sv[a]==c['display'],(k,a)
  target=cells[a];common=ref(a)[0]<={'SUVC':11,'SITC':13,'SSTC':10}[sk]
  font=sf[int(sx[int(sc[a].get('s','0'))].get('fontId','0'))] if common else fonts[int(xfs[int(target.get('s','0'))].get('fontId','0'))]
  rich(target,c,font)
  assert not any(not ''.join(r.xpath('.//x:t/text()',namespaces=N)) for r in target.findall('x:is/x:r',N))
  cn,rn=ref(a);merge=next((m for m in merges if m.split(':')[0]==a),None);ec,er=ref(merge.split(':')[-1]) if merge else (cn,rn)
  need=required_height(c['display'],sum(widths.get(i,13) for i in range(cn,ec+1)));available=sum(float(rows[r].get('ht','15')) for r in range(rn,er+1))
  if need>available:
   if need>409.5*(er-rn+1):splits.append({'key':k,'cell':a,'height_needed':need,'rows':er-rn+1})
   else:
    per=math.ceil(need/(er-rn+1))
    for r in range(rn,er+1):heights[r]=max(heights.get(r,float(rows[r].get('ht','15'))),per)
 for r,h in heights.items():rows[r].set('ht',str(h));rows[r].set('customHeight','1')
 data[member]=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
 assert all(data[x]==orig[x] for x in data if x!=member)
 save[k]=(infos,data);records.append({'key':k,'path':p['path'],'sha256_before':before,'member':member,'cells':list(p['changes']),'height_changes':heights,'all_other_package_parts_unchanged':True})
write='--write' in sys.argv
assert not(write and splits),splits
if write:
 for k,(infos,data) in save.items():
  p=Path(F[k]['path']);assert sha(p)==F[k]['hash'];tmp=p.with_name(p.name+'.date-fix')
  with zipfile.ZipFile(tmp,'w') as z:
   for i in infos:z.writestr(i,data[i.filename])
  os.replace(tmp,p);next(x for x in records if x['key']==k)['sha256_after']=sha(p)
(H/('date-integrated.json' if write else 'date-preflight.json')).write_text(json.dumps({'write':write,'files':records,'split_candidates':splits},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'write':write,'split_candidates':splits,'files':records},ensure_ascii=True))
