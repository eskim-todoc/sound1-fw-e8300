from pathlib import Path
from lxml import etree as E
import json,zipfile,math,os,sys
from integrate import sheetmap,vals,N,sha,ref,required_height,rich,tag
H=Path(__file__).resolve().parent;P=json.loads((H/'layout-scope-plan.json').read_text(encoding='utf-8'))
pkgs={}
for k,b in P.items():
 with zipfile.ZipFile(b['path']) as z:pkgs[k]=(z.infolist(),{i.filename:z.read(i.filename) for i in z.infolist()})
records=[];save={};splits=[]
for k,b in P.items():
 path=Path(b['path']);assert sha(path)==b['hash'];infos,data=pkgs[k];orig=data.copy();member=sheetmap(data)[b['sheet']];root=E.fromstring(data[member]);beforevals=vals(data,member)
 rows={int(x.get('r')):x for x in root.find('x:sheetData',N)};heights={}
 if b['changes']:
  sk='SUVC' if k=='SUVR' else k;sd=pkgs[sk][1];sr=E.fromstring(sd[sheetmap(sd)[P[sk]['sheet']]])
  ss=E.fromstring(sd['xl/styles.xml']);sf=ss.find('x:fonts',N);sx=ss.find('x:cellXfs',N)
  with zipfile.ZipFile(H/(k+'-scope-authored.xlsx')) as z:stage={i.filename:z.read(i.filename) for i in z.infolist()}
  sv=vals(stage,sheetmap(stage)[b['sheet']]);widths={}
  for col in root.findall('x:cols/x:col',N):
   for n in range(int(col.get('min')),int(col.get('max'))+1):widths[n]=float(col.get('width','13'))
  merges=[m.get('ref') for m in root.findall('x:mergeCells/x:mergeCell',N)]
  for a,c in b['changes'].items():
   assert beforevals[a]==c['before'] and sv[a]==c['display']
   target=root.find(f'.//x:c[@r="{a}"]',N);sc=sr.find(f'.//x:c[@r="{a}"]',N)
   font=sf[int(sx[int(sc.get('s','0'))].get('fontId','0'))];rich(target,c,font)
   cn,rn=ref(a);m=next((m for m in merges if m.split(':')[0]==a),None);ec,er=ref(m.split(':')[-1]) if m else (cn,rn)
   need=required_height(c['display'],sum(widths.get(n,13) for n in range(cn,ec+1)));available=sum(float(rows[r].get('ht','15')) for r in range(rn,er+1))
   if need>available:
    if need>409.5*(er-rn+1):splits.append({'key':k,'cell':a,'need':need})
    else:
     per=math.ceil(need/(er-rn+1))
     for r in range(rn,er+1):heights[r]=max(heights.get(r,float(rows[r].get('ht','15'))),per)
 for r,h in heights.items():rows[r].set('ht',str(h));rows[r].set('customHeight','1')
 if b['page_break_after_rows']:
  br=root.find('x:rowBreaks',N)
  if br is None:
   br=E.Element(tag('rowBreaks'))
   after={'colBreaks','customProperties','cellWatches','ignoredErrors','smartTags','drawing','legacyDrawing','legacyDrawingHF','picture','oleObjects','controls','webPublishItems','tableParts','extLst'}
   idx=next((i for i,x in enumerate(root) if E.QName(x).localname in after),len(root));root.insert(idx,br)
  existing={int(x.get('id')):x for x in br}
  for r in b['page_break_after_rows']:
   if r not in existing:E.SubElement(br,tag('brk'),id=str(r),min='0',max='16383',man='1')
  children=sorted(list(br),key=lambda x:int(x.get('id')))
  for x in list(br):br.remove(x)
  for x in children:br.append(x)
  br.set('count',str(len(children)));br.set('manualBreakCount',str(sum(x.get('man')=='1' for x in children)))
 data[member]=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
 aftervals=vals(data,member);assert {a:v for a,v in beforevals.items() if a not in b['changes']}=={a:v for a,v in aftervals.items() if a not in b['changes']}
 assert all(orig[x]==data[x] for x in data if x!=member)
 save[k]=(infos,data);records.append({'key':k,'path':str(path),'sha256_before':sha(path),'member':member,'changed_cells':list(b['changes']),'page_break_after_rows':b['page_break_after_rows'],'height_changes':heights,'all_other_values_and_package_members_unchanged':True})
write='--write' in sys.argv;assert not(write and splits),splits
if write:
 for k,(infos,data) in save.items():
  path=Path(P[k]['path']);assert sha(path)==P[k]['hash'];tmp=path.with_name(path.name+'.scope-layout')
  with zipfile.ZipFile(tmp,'w') as z:
   for i in infos:z.writestr(i,data[i.filename])
  os.replace(tmp,path);next(x for x in records if x['key']==k)['sha256_after']=sha(path)
(H/('layout-scope-integrated.json' if write else 'layout-scope-preflight.json')).write_text(json.dumps({'write':write,'files':records,'split_candidates':splits},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'write':write,'files':records,'split_candidates':splits},ensure_ascii=True))
