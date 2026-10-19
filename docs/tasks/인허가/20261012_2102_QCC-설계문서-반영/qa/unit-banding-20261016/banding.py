from pathlib import Path
from copy import deepcopy
import sys,json,re,shutil
H=Path(__file__).resolve().parent
sys.path.insert(0,str(H.parent/'unit-rebuild-20261016'))
from rebuild import Book,S,OUT,digest,xml,stripns,writebook
SHEET='보고서_유닛 시험'
def save(n,v):(H/n).write_text(json.dumps(v,ensure_ascii=False,indent=2),encoding='utf-8')
def prepare():
 sha=digest(OUT);shutil.copy2(OUT,H/'before.xlsx');assert digest(H/'before.xlsx')==sha
 b=Book(H/'before.xlsx');sh=b.sheets[SHEET]
 starts=sorted([(int(a[1:]),b.value(SHEET,a).strip()) for a in b.cells[SHEET] if re.fullmatch(r'D\d+',a) and b.value(SHEET,a).strip().startswith('SW-FW-UnitTest')])
 merges=[m.get('ref') for m in sh.findall('.//'+S+'mergeCell')]
 blocks=[]
 for i,(r,testid) in enumerate(starts):
  span=next((v for v in merges if v.startswith(f'D{r}:D')),None)
  end=int(span.split(':D')[1]) if span else r
  assert not any(r<s<=end for s,tid in starts)
  blocks.append({'order':i+1,'id':testid,'core':testid.split('-')[3],'start':r,'end':end,'color':'FFFFFF' if i%2==0 else 'F5F5F5'})
 assert len(blocks)==130 and blocks[0]['start']==30 and blocks[-1]['end']==1763
 assert all(blocks[i]['end']+1==blocks[i+1]['start'] for i in range(len(blocks)-1))
 for v in merges:
  nums=[int(x) for x in re.findall(r'\d+',v)]
  assert not any(nums[0]<r<=nums[-1] for r,tid in starts),v
 plan={'target':str(OUT),'sha':sha,'sheet':SHEET,'columns':'BCDEFGHIJKLM','first':30,'last':1763,'blocks':blocks}
 save('plan.json',plan)
 print(json.dumps({'sha':sha,'blocks':len(blocks),'range':'B30:M1763','transitions':[g for i,g in enumerate(blocks) if i==0 or g['core']!=blocks[i-1]['core']]},ensure_ascii=False))
def integrate():
 b=Book(H/'before.xlsx');a=Book(H/'authored.xlsx');p=json.loads((H/'plan.json').read_text(encoding='utf-8'))
 xfs=b.styles.find(S+'cellXfs');fills=b.styles.find(S+'fills');cache={}
 fids={color:next(i for i,f in enumerate(fills) if f.find(S+'patternFill/'+S+'fgColor') is not None and f.find(S+'patternFill/'+S+'fgColor').get('rgb')=='FF'+color) for color in ['FFFFFF','F5F5F5']}
 for block in p['blocks']:
  fid=fids[block['color']]
  for r in range(block['start'],block['end']+1):
   for col in p['columns']:
    addr=f'{col}{r}';c=b.cells[SHEET][addr];ac=a.cells[SHEET].get(addr)
    if ac is not None:
     af=a.styles.find(S+'fills')[int(a.styles.find(S+'cellXfs')[int(ac.get('s','0'))].get('fillId','0'))]
     fg=af.find(S+'patternFill/'+S+'fgColor');assert fg is not None and fg.get('rgb','')[-6:]==block['color'],addr
    old=int(c.get('s','0'));key=(old,fid)
    if key not in cache:
     xf=deepcopy(xfs[old]);xf.set('fillId',str(fid));xf.set('applyFill','1');sig=stripns(xf)
     idx=next((i for i,x in enumerate(xfs) if stripns(x)==sig),None)
     if idx is None:idx=len(xfs);xfs.append(xf)
     cache[key]=idx
    c.set('s',str(cache[key]))
 xfs.set('count',str(len(xfs)));b.parts['xl/styles.xml']=xml(b.styles);b.parts[b.names[SHEET]]=xml(b.sheets[SHEET]);writebook(b.parts,H/'candidate.xlsx');verify()
def verify():
 b=Book(H/'before.xlsx');a=Book(H/'candidate.xlsx');p=json.loads((H/'plan.json').read_text(encoding='utf-8'))
 assert a.parts.keys()==b.parts.keys();changed=[k for k in a.parts if a.parts[k]!=b.parts[k]]
 assert set(changed).issubset({'xl/styles.xml',b.names[SHEET]})
 ax=a.styles.find(S+'cellXfs');bx=b.styles.find(S+'cellXfs')
 assert all(stripns(x)==stripns(ax[i]) for i,x in enumerate(bx))
 aa=deepcopy(a.styles);bb=deepcopy(b.styles);aa.remove(aa.find(S+'cellXfs'));bb.remove(bb.find(S+'cellXfs'));assert stripns(aa)==stripns(bb)
 expected={f'{col}{r}':block['color'] for block in p['blocks'] for r in range(block['start'],block['end']+1) for col in p['columns']}
 assert a.cells[SHEET].keys()==b.cells[SHEET].keys()
 for addr,c in a.cells[SHEET].items():
  old=b.cells[SHEET][addr]
  if addr in expected:
   xf=deepcopy(ax[int(c.get('s','0'))]);of=deepcopy(bx[int(old.get('s','0'))])
   fg=a.styles.find(S+'fills')[int(xf.get('fillId'))].find(S+'patternFill/'+S+'fgColor');assert fg.get('rgb')=='FF'+expected[addr]
   for key in ['fillId','applyFill']:xf.attrib.pop(key,None);of.attrib.pop(key,None)
   assert stripns(xf)==stripns(of),(addr,'style')
   if 's' in old.attrib:c.set('s',old.get('s'))
   else:c.attrib.pop('s',None)
 assert stripns(a.sheets[SHEET])==stripns(b.sheets[SHEET]),'Worksheet changed beyond cell fill style'
 result={'status':'PASS','blocks':len(p['blocks']),'cells':len(expected),'onlyChangedParts':changed,'contentFormulasMergesDimensionsNonFillStyles':'unchanged','otherPartsByteIdentical':True,'candidateSHA':digest(H/'candidate.xlsx')}
 save('validation.json',result);print(json.dumps(result,ensure_ascii=False))
if __name__=='__main__':{'prepare':prepare,'integrate':integrate,'verify':verify}[sys.argv[1]]()
