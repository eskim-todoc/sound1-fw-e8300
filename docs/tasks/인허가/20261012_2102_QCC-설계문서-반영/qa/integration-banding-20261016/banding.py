from pathlib import Path
import sys,json,shutil,re
from copy import deepcopy
from lxml import etree as E
H=Path(__file__).resolve().parent
sys.path.insert(0,str(H.parent/'unit-rebuild-20261016'))
from rebuild import Book,S,digest,xml,stripns,writebook
TARGET=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/04_설계 검증/SW/외부기/[E8300-QCC] [완료] SITR-FW-TD24A 소프트웨어 통합시험 보고서.xlsx')
SHEET='보고서_통합시험'
BASE_SHA='6AD96E64B26D11ACFEB649F3752DF2B951D76EE6F78D5192AFD88E01F1A47C40'
def save(name,obj): (H/name).write_text(json.dumps(obj,ensure_ascii=False,indent=2),encoding='utf-8')
def prepare():
 assert digest(TARGET)==BASE_SHA,'Target changed before snapshot'
 shutil.copy2(TARGET,H/'before.xlsx')
 b=Book(H/'before.xlsx')
 starts=[]
 for a in b.cells[SHEET]:
  if not re.fullmatch(r'C\d+',a):continue
  m=re.fullmatch(r'\s*SW-FW-IntegrationTest-ID-(\d+)\s*',b.value(SHEET,a))
  if m:starts.append((int(m[1]),int(a[1:])))
 starts.sort()
 assert [i for i,r in starts]==list(range(1,25))
 blocks=[{'id':i,'start':r,'end':starts[j+1][1]-1 if j+1<len(starts) else 144,'color':'FFFFFF' if i%2 else 'F5F5F5'} for j,(i,r) in enumerate(starts)]
 save('plan.json',{'target':str(TARGET),'sha':BASE_SHA,'sheet':SHEET,'blocks':blocks})
 save('notices-before.json',{p:[a for a in b.cells[SHEET] if p in b.value(SHEET,a)] for p in ['[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]']})
 print(json.dumps({'tests':len(blocks),'sha':BASE_SHA}))
def integrate():
 b=Book(H/'before.xlsx');a=Book(H/'authored.xlsx');plan=json.loads((H/'plan.json').read_text(encoding='utf-8'))
 xfs=b.styles.find(S+'cellXfs');fills=b.styles.find(S+'fills');cache={};changed=[]
 fillids={color:next(i for i,f in enumerate(fills) if f.find(S+'patternFill/'+S+'fgColor') is not None and f.find(S+'patternFill/'+S+'fgColor').get('rgb')=='FF'+color) for color in ['FFFFFF','F5F5F5']}
 for block in plan['blocks']:
  fid=fillids[block['color']]
  for r in range(block['start'],block['end']+1):
   for col in 'BCDEFGHIJKLMNO':
    addr=f'{col}{r}';c=b.cells[SHEET][addr]
    # Artifact Tool authors the band colors; native OOXML preserves all other formatting.
    ac=a.cells[SHEET].get(addr)
    if ac is not None:
     af=a.styles.find(S+'fills')[int(a.styles.find(S+'cellXfs')[int(ac.get('s','0'))].get('fillId','0'))]
     fg=af.find(S+'patternFill/'+S+'fgColor')
     assert fg is not None and fg.get('rgb','')[-6:]==block['color'],(addr,'artifact fill')
    old=int(c.get('s','0'));key=(old,fid)
    if key not in cache:
     xf=deepcopy(xfs[old]);xf.set('fillId',str(fid));xf.set('applyFill','1')
     sig=stripns(xf);idx=next((i for i,x in enumerate(xfs) if stripns(x)==sig),None)
     if idx is None:idx=len(xfs);xfs.append(xf)
     cache[key]=idx
    if cache[key]!=old:changed.append(addr)
    c.set('s',str(cache[key]))
 xfs.set('count',str(len(xfs)))
 b.parts['xl/styles.xml']=xml(b.styles);b.parts[b.names[SHEET]]=xml(b.sheets[SHEET]);writebook(b.parts,H/'candidate.xlsx')
 verify()
 print(json.dumps({'changedCellStyles':len(changed),'candidateSHA':digest(H/'candidate.xlsx')}))
def verify():
 b=Book(H/'before.xlsx');a=Book(H/'candidate.xlsx');p=json.loads((H/'plan.json').read_text(encoding='utf-8'))
 assert a.parts.keys()==b.parts.keys()
 assert all(a.parts[k]==v for k,v in b.parts.items() if k not in ['xl/styles.xml',b.names[SHEET]])
 ax=a.styles.find(S+'cellXfs');bx=b.styles.find(S+'cellXfs')
 for i,x in enumerate(bx):assert stripns(x)==stripns(ax[i])
 aa=deepcopy(a.styles);bb=deepcopy(b.styles);aa.remove(aa.find(S+'cellXfs'));bb.remove(bb.find(S+'cellXfs'));assert stripns(aa)==stripns(bb)
 assert a.cells[SHEET].keys()==b.cells[SHEET].keys()
 expected={f'{col}{r}':block['color'] for block in p['blocks'] for r in range(block['start'],block['end']+1) for col in 'BCDEFGHIJKLMNO'}
 for addr,c in a.cells[SHEET].items():
  orig=b.cells[SHEET][addr];xc=deepcopy(c);xo=deepcopy(orig)
  if addr in expected:
   asf=deepcopy(ax[int(c.get('s','0'))]);bsf=deepcopy(bx[int(orig.get('s','0'))])
   fid=int(asf.get('fillId'));color=a.styles.find(S+'fills')[fid].find(S+'patternFill/'+S+'fgColor').get('rgb')
   assert color=='FF'+expected[addr]
   for attr in ['fillId','applyFill']:asf.attrib.pop(attr,None);bsf.attrib.pop(attr,None)
   assert stripns(asf)==stripns(bsf),(addr,'other style changed')
   xc.attrib.pop('s',None);xo.attrib.pop('s',None)
  assert stripns(xc)==stripns(xo),(addr,'content/attributes changed')
 ash=deepcopy(a.sheets[SHEET]);bsh=deepcopy(b.sheets[SHEET]);ash.remove(ash.find(S+'sheetData'));bsh.remove(bsh.find(S+'sheetData'));assert stripns(ash)==stripns(bsh)
 assert [dict(r.attrib) for r in a.sheets[SHEET].find(S+'sheetData')]==[dict(r.attrib) for r in b.sheets[SHEET].find(S+'sheetData')]
 save('validation.json',{'status':'PASS','blocks':24,'cellsChecked':len(expected),'contentsAndNonFillStyles':'unchanged','mergesRowHeightsColumnWidthsPrintSettings':'unchanged','otherZipParts':'byte-identical','candidateSHA':digest(H/'candidate.xlsx')})
if __name__=='__main__': {'prepare':prepare,'integrate':integrate,'verify':verify}[sys.argv[1]]()
