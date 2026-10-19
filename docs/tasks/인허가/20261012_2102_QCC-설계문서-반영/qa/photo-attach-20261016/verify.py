import package as m
from lxml import etree as E
import json,hashlib,re

q=m.Book(m.HERE/'qcc-source.xlsx');p=m.Book(m.HERE/'personal-source.xlsx');b=m.Book(m.HERE/'candidate.xlsx')
pl=m.readj('plan.json');audit=m.readj('edit-audit.json');errors=[];checks={}
checks['testOrder']=[x['id'] for x in b.tests()]
if checks['testOrder']!=[x['id'] for x in q.tests()]:errors.append(['test-order'])
covered={}
for mm in b.sheets[m.MAIN].findall('s:mergeCells/s:mergeCell',m.NS):
 c1,r1,c2,r2=m.span(mm.get('ref'))
 for row in range(r1,r2+1):
  for cc in range(ord(c1),ord(c2)+1):
   a=chr(cc)+str(row)
   if a in covered:errors.append(['merge-overlap',a])
   covered[a]=mm.get('ref')
   if a!=c1+str(r1) and b.value(a).strip():errors.append(['hidden-content',a])
for old,ch in pl['changes'].items():
 if b.value(ch['newAddress'])!=ch['newText']:errors.append(['changed-value',old,ch['newAddress']])
preserved=0
for a,c in q.cells[m.MAIN].items():
 col,row=m.ref(a)
 if str(row) not in pl['rowMap'] or a in pl['changes']:continue
 na=col+str(pl['rowMap'][str(row)]['first'])
 if q.value(a).strip()!=b.value(na).strip():errors.append(['preserved-value',a,na])
 else:preserved+=1
for a,c in b.cells[m.MAIN].items():
 v=b.value(a);col,row=m.ref(a)
 if '[+]' in v or '[-]' in v:errors.append(['comparison-in-body',a])
 if 34<=row<=pl['rowMap']['284']['last'] and col in 'BCDEFGHIJKL' and v.strip():
  if not v.startswith('\n') or not v.endswith('\n'):errors.append(['padding',a])
  al=b.xfs[int(c.get('s','0'))].find(m.t('alignment'))
  old=next((v['old'] for v in pl['rows'] if v['new']==row),None)
  mp=next((v for v in audit['testRowMapping'] if old and v['qccStart']<=old<=v['qccEnd']),None)
  pc=p.cells[m.MAIN].get(col+str(old+mp['privateMinusQccOffset'])) if mp else None
  want=p.xfs[int(pc.get('s','0'))].find(m.t('alignment')).get('horizontal') if pc is not None and p.xfs[int(pc.get('s','0'))].find(m.t('alignment')) is not None else None
  if want and (al is None or al.get('horizontal')!=want):errors.append(['alignment',a,want])
draw=E.fromstring(b.data['xl/drawings/drawingPhotoAttach.xml'])
rels=E.fromstring(b.data['xl/drawings/_rels/drawingPhotoAttach.xml.rels']);rd={n.get('Id'):n.get('Target') for n in rels}
if len(draw)!=123:errors.append(['photo-count',len(draw)])
picchecks=[]
for anc,photo in zip(draw,pl['photos']):
 ex=anc.find('x:ext',m.NS);fr=anc.find('x:from',m.NS)
 row=int(fr.findtext('x:row',namespaces=m.NS))+1;col=int(fr.findtext('x:col',namespaces=m.NS))
 rid=anc.find('.//a:blip',m.NS).get('{'+m.R+'}embed');path=m.posixpath.normpath('xl/drawings/'+rd[rid])
 sha=hashlib.sha256(b.data[path]).hexdigest().upper()
 if sha!=photo['sha256'].upper():errors.append(['photo-sha',photo['name']])
 if row!=photo['row'] or col!=10 or ex.get('cy')!='2880000':errors.append(['photo-geometry',photo['name']])
 rowheight=next(v['height'] for v in pl['rows'] if v['new']==row)
 endpt=(int(fr.findtext('x:rowOff',namespaces=m.NS))+int(ex.get('cy')))/12700
 if endpt>rowheight+0.01:errors.append(['photo-overflow',photo['name'],endpt,rowheight])
 picchecks.append({'name':photo['name'],'cell':'K'+str(row),'heightCm':8,'sha256':sha})
checks.update(preservedCells=preserved,changedCells=len(pl['changes']),greenCells=sum(bool(v.get('greenRanges')) for v in audit['events']),deletedRows=pl['deleteRows'],photos=picchecks)
unchangedparts=[]
for name,path in q.paths.items():
 if name not in [m.MAIN,m.COMP,*pl.get('otherChanges',{})]:
  if q.data[path]!=b.data[path]:errors.append(['unexpected-sheet-change',name])
  else:unchangedparts.append(name)
checks['unchangedSheets']=unchangedparts
for sheet,items in pl.get('otherChanges',{}).items():
 for old,ch in items.items():
  if b.value(ch['newAddress'],sheet)!=ch['newText']:errors.append(['support-text',sheet,old,ch['newAddress']])
checks['sourceHashes']={'base':m.digest(q.path),'personal':m.digest(p.path)}
checks['unresolved']=pl['decisions'].get('pendingClarifications',[])
result={'candidateSHA256':m.digest(b.path),'errors':errors,'checks':checks,'scope':'저장 문서의 내용·서식·사진 연결 검사. 새 제품 시험 실행 또는 승인 아님.'}
m.savej('validation.json',result)
print(json.dumps({'errors':errors,'photos':len(picchecks),'tests':len(checks['testOrder']),'unchangedSheets':unchangedparts},ensure_ascii=True))
if errors:raise SystemExit(1)
