from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from lxml import etree as E
from copy import deepcopy
import json,re,hashlib,sys

H=Path(__file__).resolve().parent
G=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/04_설계 검증/SW/외부기')
BASE=G/'김은수_작성/[E8300] SUVR-FW-TD24A 소프트웨어 유닛 검증 보고서.xlsx'
QCC=G/'[E8300-QCC] SUVR-FW-TD24A 소프트웨어 유닛 검증 보고서.xlsx'
OUT=G/'[E8300-QCC] SUVR-FW-TD24A 소프트웨어 유닛 검증 보고서_재작성.xlsx'
NS={'s':'http://schemas.openxmlformats.org/spreadsheetml/2006/main','r':'http://schemas.openxmlformats.org/officeDocument/2006/relationships'}
S='{'+NS['s']+'}'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def savej(name,v): (H/name).write_text(json.dumps(v,ensure_ascii=False,indent=2),encoding='utf-8')
def readj(name):return json.loads((H/name).read_text(encoding='utf-8'))
def xml(x):return E.tostring(x,xml_declaration=True,encoding='UTF-8',standalone=True)
def stripns(x):
 x=deepcopy(x)
 for y in x.iter():y.tag=E.QName(y).localname
 E.cleanup_namespaces(x)
 return E.tostring(x).decode()
def writebook(parts,path):
 with ZipFile(path,'w',ZIP_DEFLATED) as z:
  for n,data in parts.items():z.writestr(n,data)
def shiftref(ref,delta,threshold=1727):
 return re.sub(r'(\$?[A-Z]+\$?)(\d+)',lambda m:m[1]+str(int(m[2])+delta if int(m[2])>=threshold else int(m[2])),ref)

def prepare():
 b=Book(H/'base.xlsx');q=Book(H/'qcc.xlsx');n='보고서_유닛 시험';sh=b.sheets[n]
 for row in sh.find(S+'sheetData'):
  if int(row.get('r'))>=1727:
   row.set('r',str(int(row.get('r'))+38))
   for c in row:c.set('r',shiftref(c.get('r'),38))
 for merge in sh.findall('.//'+S+'mergeCell'):merge.set('ref',shiftref(merge.get('ref'),38))
 sh.find(S+'dimension').set('ref',shiftref(sh.find(S+'dimension').get('ref'),38))
 for dn in b.wb.findall('.//'+S+'definedName'):
  if dn.get('name')=='_xlnm.Print_Area' and dn.get('localSheetId')=='1':dn.text=shiftref(dn.text,38)
 b.parts[b.names[n]]=xml(sh);b.parts['xl/workbook.xml']=xml(b.wb)
 writebook(b.parts,H/'prepared.xlsx')
 merges=[x.get('ref') for x in q.sheets[n].findall('.//'+S+'mergeCell') if 1727<=int(re.search(r'\d+',x.get('ref'))[0])<=1764]
 newvals={a:q.value(n,a) for a in q.cells[n] if 1727<=int(re.search(r'\d+',a)[0])<=1764 and re.match(r'^[B-M]\d+$',a) and q.value(n,a)}
 updates={'D19':'\nQCC 유닛 시험에는 QCC SDK, PSTool, nRF Connect, CRA 앱과 실제 로그/전송 바이트 수집 도구를 사용한다.\n[QCC 자료 입력] 실제 SDK, 소스/도구, 회로/프로토콜 버전 및 실행 결과와 수집 자료.\n','D24':'\n시험자가 기존 E8300 시험 전 항목의 통과를 확인했다. 입력별 예상 출력과 대조하여 결과 칸을 정리했다. QCC 유닛 시험의 실행 및 판정 상태는 각 시험 결과와 통과 여부 칸에 기재한다.\n'}
 ids=[];cur='';changes=[]
 for r in range(1727,1765):
  if newvals.get('D'+str(r)):cur=newvals['D'+str(r)].strip();ids.append(cur)
  cond=re.search(r'조건\s*\[([^]]+)\]',newvals.get('K'+str(r),''))
  for col in 'BCDEFGHIJKLM':
   a=col+str(r)
   if a in newvals:changes.append({'id':cur,'condition':cond[1] if cond else '', 'address':a,'field':b.value(n,col+'28').strip(),'before':'','after':newvals[a]})
 for a,v in updates.items():changes.append({'id':'공통 안내','condition':'','address':a,'field':b.value(n,'C'+a[1:]).strip(),'before':b.value(n,a),'after':v})
 plan={'sheet':n,'start':1727,'end':1764,'ids':ids,'values':newvals,'updates':updates,'merges':merges,'rowHeights':{r.get('r'):float(r.get('ht','270')) for r in q.sheets[n].find(S+'sheetData') if 1727<=int(r.get('r'))<=1764},'changes':changes,'comparisonSheet':n+'_비교'}
 savej('plan.json',plan)
 print(json.dumps({'ids':len(ids),'newCells':len(newvals),'changes':len(changes),'candidate':'prepared.xlsx'},ensure_ascii=True))

class StyleImport:
 def __init__(self,dst,src):self.dst=dst;self.src=src;self.maps={}
 def part(self,group,i):
  key=(group,i)
  if key in self.maps:return self.maps[key]
  source=self.src.styles.find(S+group);dest=self.dst.styles.find(S+group)
  el=deepcopy(source[i])
  if group in ['cellXfs','cellStyleXfs']:
   for a,g in [('fontId','fonts'),('fillId','fills'),('borderId','borders')]:
    if a in el.attrib:el.set(a,str(self.part(g,int(el.get(a)))))
   if group=='cellXfs' and 'xfId' in el.attrib:el.set('xfId',str(self.part('cellStyleXfs',int(el.get('xfId')))))
   if int(el.get('numFmtId','0'))>=164:
    nf=int(el.get('numFmtId'));s=self.src.styles.find(S+'numFmts');d=self.dst.styles.find(S+'numFmts')
    match=next(x for x in s if int(x.get('numFmtId'))==nf)
    existing=next((x for x in d if x.get('formatCode')==match.get('formatCode')),None)
    if existing is None:
     existing=deepcopy(match);existing.set('numFmtId',str(max([163]+[int(x.get('numFmtId')) for x in d])+1));d.append(existing);d.set('count',str(len(d)))
    el.set('numFmtId',existing.get('numFmtId'))
  sig=stripns(el);idx=next((j for j,x in enumerate(dest) if stripns(x)==sig),None)
  if idx is None:idx=len(dest);dest.append(el);dest.set('count',str(len(dest)))
  self.maps[key]=idx;return idx
 def cell(self,c):
  c=deepcopy(c);c.set('s',str(self.part('cellXfs',int(c.get('s','0')))))
  if c.get('t')=='s':
   si=deepcopy(self.src.strings[int(c.find(S+'v').text)]);si.tag=S+'is'
   c.remove(c.find(S+'v'));c.append(si);c.set('t','inlineStr')
  return c

def integrate():
 b=Book(H/'prepared.xlsx');q=Book(H/'qcc.xlsx');auth=Book(H/'authored.xlsx');p=readj('plan.json');n=p['sheet'];sh=b.sheets[n];sd=sh.find(S+'sheetData');qi=StyleImport(b,q);ai=StyleImport(b,auth)
 # Artifact Tool authors all values. Restore the source's exact rich-text and native styles.
 for row in q.sheets[n].find(S+'sheetData'):
  r=int(row.get('r'))
  if not 1727<=r<=1764:continue
  nr=deepcopy(row)
  if 's' in nr.attrib:nr.set('s',str(qi.part('cellXfs',int(nr.get('s')))))
  for c in list(nr):nr.remove(c)
  for c in row:
   a=c.get('r')
   if not re.match(r'^[A-M]\d+$',a):continue
   if q.value(n,a):assert auth.value(n,a)==q.value(n,a),(a,'artifact value differs')
   nr.append(qi.cell(c))
  nr.set('spans','1:13')
  ix=next((i for i,x in enumerate(sd) if int(x.get('r'))>r),len(sd));sd.insert(ix,nr)
 merges=sh.find(S+'mergeCells')
 for ref in p['merges']:E.SubElement(merges,S+'mergeCell',ref=ref)
 merges.set('count',str(len(merges)))
 for a,text in p['updates'].items():
  assert auth.value(n,a)==text
  old=b.cells[n][a];new=deepcopy(old)
  for x in list(new):new.remove(x)
  new.set('t','inlineStr');content=E.SubElement(new,S+'is')
  if a=='D19':
   pieces=text.split('[QCC 자료 입력]',1)
   for value,notice in [(pieces[0],False),('[QCC 자료 입력]'+pieces[1],True)]:
    run=E.SubElement(content,S+'r');rp=E.SubElement(run,S+'rPr');E.SubElement(rp,S+'rFont',val='Calibri');E.SubElement(rp,S+'sz',val='13' if notice else '11');E.SubElement(rp,S+'color',rgb='FFC00000' if notice else 'FF000000')
    if notice:E.SubElement(rp,S+'u')
    t=E.SubElement(run,S+'t');t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=value
  else:
   t=E.SubElement(content,S+'t');t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=text
  old.getparent().replace(old,new)
 cmp=deepcopy(auth.sheets[p['comparisonSheet']])
 for c in list(cmp.findall('.//'+S+'sheetData/'+S+'row/'+S+'c')):c.getparent().replace(c,ai.cell(c))
 for row in cmp.findall('.//'+S+'sheetData/'+S+'row'):
  if 's' in row.attrib:row.set('s',str(ai.part('cellXfs',int(row.get('s')))))
 for col in cmp.findall('.//'+S+'cols/'+S+'col'):
  if 'style' in col.attrib:col.set('style',str(ai.part('cellXfs',int(col.get('style')))))
 # New sheet has no workbook-external dependencies or drawings.
 for tag in ['drawing','legacyDrawing','pageSetup','pageMargins','headerFooter']:
  for el in cmp.findall(S+tag):cmp.remove(el)
 E.SubElement(cmp,S+'pageMargins',left='0.25',right='0.25',top='0.5',bottom='0.5',header='0.2',footer='0.2')
 E.SubElement(cmp,S+'pageSetup',orientation='landscape',paperSize='9',fitToWidth='1',fitToHeight='0')
 pr=cmp.find(S+'sheetPr')
 if pr is None:pr=E.Element(S+'sheetPr');cmp.insert(0,pr)
 E.SubElement(pr,S+'pageSetUpPr',fitToPage='1')
 relpath='xl/_rels/workbook.xml.rels';rels=E.fromstring(b.parts[relpath]);rid='rId'+str(max(int(x.get('Id').removeprefix('rId')) for x in rels)+1)
 E.SubElement(rels,'{http://schemas.openxmlformats.org/package/2006/relationships}Relationship',Id=rid,Type=NS['r']+'/worksheet',Target='worksheets/sheet3.xml')
 sn=E.SubElement(b.wb.find(S+'sheets'),S+'sheet',name=p['comparisonSheet'],sheetId='3');sn.set('{'+NS['r']+'}id',rid)
 dn=b.wb.find(S+'definedNames');end=len(p['changes'])+4
 E.SubElement(dn,S+'definedName',name='_xlnm.Print_Area',localSheetId='2').text="'"+p['comparisonSheet']+"'!$B$2:$G$"+str(end)
 E.SubElement(dn,S+'definedName',name='_xlnm.Print_Titles',localSheetId='2').text="'"+p['comparisonSheet']+"'!$4:$4"
 ct=E.fromstring(b.parts['[Content_Types].xml']);E.SubElement(ct,'{http://schemas.openxmlformats.org/package/2006/content-types}Override',PartName='/xl/worksheets/sheet3.xml',ContentType='application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml')
 b.parts['[Content_Types].xml']=xml(ct);b.parts[relpath]=xml(rels);b.parts[b.names[n]]=xml(sh);b.parts['xl/styles.xml']=xml(b.styles);b.parts['xl/workbook.xml']=xml(b.wb);b.parts['xl/worksheets/sheet3.xml']=xml(cmp)
 app=E.fromstring(b.parts['docProps/app.xml']);an={'a':app.nsmap[None],'v':'http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes'}
 vt=app.find('a:TitlesOfParts/v:vector',an)
 if vt is not None:E.SubElement(vt,'{'+an['v']+'}lpstr').text=p['comparisonSheet'];vt.set('size',str(len(vt)))
 for pair in app.findall('a:HeadingPairs/v:vector/v:variant',an):
  x=pair.find('v:i4',an)
  if x is not None and x.text=='2':x.text='3'
 b.parts['docProps/app.xml']=xml(app)
 writebook(b.parts,H/'candidate.xlsx')
 print(json.dumps({'candidateSHA':digest(H/'candidate.xlsx'),'size':(H/'candidate.xlsx').stat().st_size},ensure_ascii=True))
class Book:
 def __init__(self,p):
  self.path=Path(p)
  with ZipFile(p) as z:self.parts={n:z.read(n) for n in z.namelist()}
  self.wb=E.fromstring(self.parts['xl/workbook.xml']);self.styles=E.fromstring(self.parts['xl/styles.xml'])
  self.strings=list(E.fromstring(self.parts['xl/sharedStrings.xml'])) if 'xl/sharedStrings.xml' in self.parts else []
  rel={r.get('Id'):r.get('Target').lstrip('/') for r in E.fromstring(self.parts['xl/_rels/workbook.xml.rels'])}
  self.names={n.get('name'):('xl/'+rel[n.get('{'+NS['r']+'}id')] if not rel[n.get('{'+NS['r']+'}id')].startswith('xl/') else rel[n.get('{'+NS['r']+'}id')]) for n in self.wb.find(S+'sheets')}
  self.sheets={n:E.fromstring(self.parts[p]) for n,p in self.names.items()}
  self.cells={n:{c.get('r'):c for c in sh.findall('.//'+S+'sheetData/'+S+'row/'+S+'c')} for n,sh in self.sheets.items()}
 def value(self,n,a):
  c=self.cells[n].get(a)
  if c is None:return ''
  t=c.get('t');v=c.find(S+'v')
  if t=='s':return ''.join(self.strings[int(v.text)].itertext())
  if t=='inlineStr':return ''.join(c.find(S+'is').itertext())
  return v.text if v is not None else ''
 def summary(self):
  out={}
  for n,s in self.sheets.items():
   texts={a:self.value(n,a) for a in self.cells[n]}
   out[n]={'dimension':s.find(S+'dimension').get('ref') if s.find(S+'dimension') is not None else None,'rows':len(s.findall('.//'+S+'sheetData/'+S+'row')),'merges':[x.get('ref') for x in s.findall('.//'+S+'mergeCell')],'ids':{a:v for a,v in texts.items() if re.search(r'(SU-ID-|Test-ID|QCC-\d)',v)},'tail':{a:v for a,v in texts.items() if int(re.search(r'\d+',a)[0])>1680 and v},'header':{a:v for a,v in texts.items() if int(re.search(r'\d+',a)[0])<34 and v},'formulas':{a:E.tostring(c.find(S+'f')).decode() for a,c in self.cells[n].items() if c.find(S+'f') is not None},'drawings':s.xpath('count(.//s:drawing)',namespaces=NS)}
  return out
if __name__=='__main__':
 if sys.argv[1]=='prepare':prepare()
 if sys.argv[1]=='integrate':integrate()
 if sys.argv[1]=='styles':
  for p in ['base','qcc']:
   b=Book(H/(p+'.xlsx'));n='보고서_유닛 시험'
   result={}
   for col in 'BCDEFGHIJKLM':
    a=col+('1686' if p=='base' else '1727');c=b.cells[n][a];xf=b.styles.find(S+'cellXfs')[int(c.get('s','0'))]
    result[a]={'xf':stripns(xf),'font':stripns(b.styles.find(S+'fonts')[int(xf.get('fontId'))]),'fill':stripns(b.styles.find(S+'fills')[int(xf.get('fillId'))])}
   print(json.dumps({p:result},ensure_ascii=True))
 if sys.argv[1]=='inspect':
  assert BASE.exists() and QCC.exists() and not OUT.exists()
  for name,p in [('base',BASE),('qcc',QCC)]:
   snap=H/(name+'.xlsx')
   if not snap.exists():snap.write_bytes(p.read_bytes())
   assert digest(snap)==digest(p)
   b=Book(snap);savej(name+'-inspection.json',b.summary())
   print(json.dumps({'name':name,'sha':digest(p),'sheets':{n:{k:v for k,v in d.items() if k in ['dimension','rows','drawings','formulas']} for n,d in b.summary().items()}},ensure_ascii=True))
  savej('baseline.json',{'base':str(BASE),'baseSHA':digest(BASE),'qcc':str(QCC),'qccSHA':digest(QCC),'output':str(OUT)})
