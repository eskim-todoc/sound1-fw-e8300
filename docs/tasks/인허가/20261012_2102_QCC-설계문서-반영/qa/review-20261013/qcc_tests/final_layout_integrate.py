from pathlib import Path
from lxml import etree as E
import json,zipfile,copy,math,os,sys
from integrate import sheetmap,vals,N,NS,sha,ref,required_height,rich,tag,texts
H=Path(__file__).resolve().parent;P=json.loads((H/'final-layout-plan.json').read_text(encoding='utf-8'));records=[];pkgs={}
for k,b in P.items():
 path=Path(b['path']);assert sha(path)==b['hash'],(k,'concurrent change')
 with zipfile.ZipFile(path) as z:infos=z.infolist();data={i.filename:z.read(i.filename) for i in infos}
 orig=data.copy();root=E.fromstring(data[b['member']]);before=vals(data,b['member']);styles=E.fromstring(data['xl/styles.xml']);xfs=styles.find('x:cellXfs',N);fonts=styles.find('x:fonts',N);stylechanges=[];heightchanges={}
 with zipfile.ZipFile(H/(k+'-final-layout-authored.xlsx')) as z:stage={i.filename:z.read(i.filename) for i in z.infolist()}
 sv=vals(stage,sheetmap(stage)[b['sheet']]);rows={int(r.get('r')):r for r in root.find('x:sheetData',N)};default=float(root.find('x:sheetFormatPr',N).get('defaultRowHeight','15'))
 for a,c in b['changes'].items():
  assert before[a]==c['before'] and sv[a]==c['display'],(k,a,'artifact or concurrent data mismatch')
  target=root.find(f'.//x:c[@r="{a}"]',N)
  if c.get('punctuation_only'):
   found=0
   for tt in target.findall('.//x:t',N):
    if c['old'] in (tt.text or ''):found+=tt.text.count(c['old']);tt.text=tt.text.replace(c['old'],c['new'])
   assert found==1 and texts(target)==c['display']
  else:
   font=fonts[int(xfs[int(target.get('s','0'))].get('fontId','0'))];rich(target,c,font)
   col=ref(a)[0];width=next(float(x.get('width','13')) for x in root.findall('x:cols/x:col',N) if int(x.get('min'))<=col<=int(x.get('max')))
   need=required_height(c['display'],width);row=ref(a)[1];available=float(rows[row].get('ht',str(default)))
   assert need<=409.5,(k,a,need,'requires common row split')
   if need>available:heightchanges[row]=math.ceil(need)
 for a in b['top_alignment_cells']:
  target=root.find(f'.//x:c[@r="{a}"]',N);oldidx=int(target.get('s','0'));xf=copy.deepcopy(xfs[oldidx]);al=xf.find('x:alignment',N)
  if al is None:al=E.SubElement(xf,tag('alignment'))
  al.set('horizontal','left');al.set('vertical','top');xf.set('applyAlignment','1')
  same=next((i for i,x in enumerate(xfs) if E.tostring(x)==E.tostring(xf)),None)
  if same is None:same=len(xfs);xfs.append(xf)
  target.set('s',str(same));stylechanges.append({'cell':a,'old_style':oldidx,'new_style':same,'alignment':'left/top','other_xf_attributes_unchanged':True})
 for r,h in {**b['heights'],**{str(r):v for r,v in heightchanges.items()}}.items():
  row=rows[int(r)];heightchanges[int(r)]={'before':float(row.get('ht',str(default))),'after':h};row.set('ht',str(h));row.set('customHeight','1')
 br=root.find('x:rowBreaks',N)
 if br is None:
  br=E.Element(tag('rowBreaks'));after={'colBreaks','customProperties','cellWatches','ignoredErrors','smartTags','drawing','legacyDrawing','legacyDrawingHF','picture','oleObjects','controls','webPublishItems','tableParts','extLst'};idx=next((i for i,x in enumerate(root) if E.QName(x).localname in after),len(root));root.insert(idx,br)
 oldbreaks=[dict(x.attrib) for x in br];existing={int(x.get('id')) for x in br}
 for row in b['page_break_before_rows']:
  if row-1 not in existing:E.SubElement(br,tag('brk'),id=str(row-1),min='0',max='16383',man='1')
 children=sorted(list(br),key=lambda x:int(x.get('id')))
 for x in list(br):br.remove(x)
 for x in children:br.append(x)
 br.set('count',str(len(children)));br.set('manualBreakCount',str(sum(x.get('man')=='1' for x in children)))
 data[b['member']]=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
 if stylechanges:xfs.set('count',str(len(xfs)));data['xl/styles.xml']=E.tostring(styles,xml_declaration=True,encoding='UTF-8',standalone=True)
 afterv=vals(data,b['member']);assert {a:v for a,v in before.items() if a not in b['changes']}=={a:v for a,v in afterv.items() if a not in b['changes']}
 assert all(orig[m]==data[m] for m in data if m not in [b['member'],'xl/styles.xml'])
 assert root.find('x:mergeCells',N) is None or E.tostring(root.find('x:mergeCells',N))==E.tostring(E.fromstring(orig[b['member']]).find('x:mergeCells',N))
 assert not root.xpath('//x:r[not(x:t/text())]',namespaces=N),(k,'empty native-rich run')
 candidate=Path(b['candidate_path']);candidate.parent.mkdir(parents=True,exist_ok=True)
 with zipfile.ZipFile(candidate,'w') as z:
  for i in infos:z.writestr(i,data[i.filename])
 pkgs[k]=(data,afterv,root)
 records.append({'key':k,'path':b['path'],'candidate':str(candidate),'sha256_before':b['hash'],'sha256_candidate':sha(candidate),'height_changes':heightchanges,'alignment_changes':stylechanges,'text_changes':{a:{'before':c['before'],'display':c['display'],'current':c['current'],'punctuation_only':c.get('punctuation_only',False)} for a,c in b['changes'].items()},'breaks_before':oldbreaks,'breaks_after':[dict(x.attrib) for x in br],'all_other_values_formulas_and_package_members_unchanged':True,'merge_structure_unchanged':True,'native_display':'미실행: final candidate print requires root native engine'})
for ck,rk,last,start,end in [('SUVC','SUVR',11,30,1764),('SITC','SITR',13,25,144),('SSTC','SSTR',10,34,285)]:
 for a,v in pkgs[ck][1].items():
  c,r=ref(a)
  if 2<=c<=last and start<=r<=end:assert v==pkgs[rk][1].get(a,''),(ck,a,'common')
 assert [dict(x.attrib) for x in pkgs[ck][2].find('x:rowBreaks',N)]==[dict(x.attrib) for x in pkgs[rk][2].find('x:rowBreaks',N)],(ck,'manual breaks')
(H/'final-layout-candidates.json').write_text(json.dumps({'files':records,'common_values_merges_and_page_breaks':'통과','new_or_changed_product_results':False},ensure_ascii=False,indent=2),encoding='utf-8')
if '--write' in sys.argv:
 for rec in records:
  path=Path(rec['path']);assert sha(path)==rec['sha256_before']
  backup=H/'final-layout-before'/path.name;backup.parent.mkdir(parents=True,exist_ok=True)
  if backup.exists():assert sha(backup)==rec['sha256_before'],(rec['key'],'backup differs')
  else:backup.write_bytes(path.read_bytes())
  rec['backup']=str(backup)
 for rec in records:
  path=Path(rec['path']);assert sha(path)==rec['sha256_before'];candidate=Path(rec['candidate']);assert sha(candidate)==rec['sha256_candidate'];tmp=path.with_name(path.name+'.final-layout');tmp.write_bytes(candidate.read_bytes());os.replace(tmp,path)
  rec['sha256_after']=sha(path)
  k=rec['key'];book=json.loads((H/'author-payload.json').read_text(encoding='utf-8'))
  for a,c in P[k]['changes'].items():
   sn=P[k]['sheet']
   if not c.get('punctuation_only'):book['books'][k]['comparison'].setdefault(sn,{})[a]=c
   elif a in book['books'][k]['comparison'].get(sn,{}):
    cc=book['books'][k]['comparison'][sn][a];cc['display']=cc['display'].replace(c['old'],c['new']);cc['current']=cc['current'].replace(c['old'],c['new'])
    for line in cc.get('lines',[]):
     if line['status']!='old':line['text']=line['text'].replace(c['old'],c['new'])
  (H/'author-payload.json').write_text(json.dumps(book,ensure_ascii=False,indent=2),encoding='utf-8')
 (H/'final-layout-applied.json').write_text(json.dumps({'files':records,'common_values_merges_and_page_breaks':'통과','new_or_changed_product_results':False},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'write':'--write' in sys.argv,'files':[{'key':r['key'],'candidate':r['candidate'],'sha256':r['sha256_candidate'],'height_changes':r['height_changes'],'alignment':r['alignment_changes']} for r in records]},ensure_ascii=True))
