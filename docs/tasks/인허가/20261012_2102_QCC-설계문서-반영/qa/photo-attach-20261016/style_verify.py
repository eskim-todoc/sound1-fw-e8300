import package as m
import style_edit as e
from lxml import etree as E
import json,re,sys

b=m.Book(m.HERE/'style-after.xlsx');a=m.Book(m.HERE/'style-before.xlsx');pl=m.readj('style-plan.json');pink=set(pl['pinkIDs']);errors=[]
testrow={r:t['id'] for t in b.tests() for r in range(t['start'],t['end']+1)};counts={'bodyStyle10':0,'bodyRun10':0,'bodyPadding':0,'outsideTextCells':0,'pinkTestIDs':len(pink)}
for sh,cells in b.cells.items():
 for addr,c in cells.items():
  col,row=m.ref(addr);body=sh==m.MAIN and 34<=row<=293;rs=b.runs(c);s=m.text(rs);istext=c.get('t') in ['s','str','inlineStr']
  if body:
   f=m.fontdict(b.fonts[int(b.xfs[int(c.get('s','0'))].get('fontId','0'))])
   if float(f.get('sz',{}).get('val',0))!=10:errors.append(('cellFont',addr,f))
   else:counts['bodyStyle10']+=1
   for r in rs:
    if float(r['font'].get('sz',{}).get('val',0))!=10:errors.append(('richFont',addr))
    else:counts['bodyRun10']+=1
   if s and istext:
    if not(s.startswith('\n') and s.endswith('\n')) or s.startswith('\n\n') or s.endswith('\n\n'):errors.append(('bodyPadding',addr))
    else:counts['bodyPadding']+=1
   if testrow.get(row) in pink:
    if '[QCC/E8300 자료 입력]' in s or (col=='K' and '[E8300 보완]' in s):errors.append(('removedNoteRemains',addr))
    if any(e.green(r['font']) for r in rs):errors.append(('greenUnderline',addr))
  elif istext:
   counts['outsideTextCells']+=1
   if s.startswith(('\r','\n')) or s.endswith(('\r','\n')):errors.append(('outsidePadding',sh,addr))
for sh in b.paths:
 if not sh.endswith('_비교'):
  av=[v.get('ref') for v in a.sheets[sh].findall('s:mergeCells/s:mergeCell',m.NS)];bv=[v.get('ref') for v in b.sheets[sh].findall('s:mergeCells/s:mergeCell',m.NS)]
  if av!=bv:errors.append(('merges',sh))
assert len(b.tests())==83
if a.tests()!=b.tests():errors.append(('testIdRowsOrder',))
for addr,c in a.cells[m.MAIN].items():
 col,row=m.ref(addr)
 if 34<=row<=293 and testrow[row] not in pink and a.value(addr).strip('\r\n')!=b.value(addr).strip('\r\n'):errors.append(('nonpinkContent',addr))
for part,blob in a.data.items():
 if part.startswith('xl/media/') and b.data.get(part)!=blob:errors.append(('mediaChanged',part))
for part in ['xl/drawings/drawing2.xml','xl/drawings/_rels/drawing1.xml.rels','xl/drawings/_rels/drawing2.xml.rels']:
 if a.data[part]!=b.data[part]:errors.append(('otherDrawingChanged',part))
for key in ['I158','J170','K159','K170']:
 if a.value(key).strip()!=b.value(key).strip():errors.append(('confirmationRegression',key))
for key in ['K179','K160','K161','K180','K278']:
 if a.value(key).strip()!=b.value(key).strip():errors.append(('existingCorrectionRegression',key))
for sh,cs in a.cells.items():
 if sh==m.MAIN or sh=='시험 보조자료':continue
 for addr,c in cs.items():
  if a.value(addr,sh).strip('\r\n')!=b.value(addr,sh).strip('\r\n'):errors.append(('otherSheetContent',sh,addr))
for r in b.sheets[m.MAIN].find(m.t('sheetData')):
 if 34<=int(r.get('r'))<=293 and float(r.get('ht','0'))>409.5:errors.append(('rowMax',r.get('r')))
for sh,cs in b.cells.items():
 if not sh.endswith('_비교'):
  for addr,c in cs.items():
   if '[+]' in b.value(addr,sh) or '[-]' in b.value(addr,sh):errors.append(('comparisonInCurrent',sh,addr))
data={'baselineSHA':m.digest(a.path),'candidateSHA':m.digest(b.path),'counts':counts,'errors':errors,'testIDs':83,'mainRows':[34,293],'pureQCCPendingPreserved':[54,59],'verdictSourceTests':[15,16,69,74,78],'newProductMeasurementsPerformed':False}
m.savej('style-validation.json',data);print(json.dumps(data,ensure_ascii=True));assert not errors
