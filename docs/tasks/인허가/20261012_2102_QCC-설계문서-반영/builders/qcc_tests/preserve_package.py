"""Selective OOXML package preservation around cells authored by Artifact Tool.
No content is authored here: values are recovered and verified from Artifact exports.
Native styles, drawings, relationships and unrelated sheets stay in the copied packages.
"""
import json,re,zipfile,hashlib,copy,os,sys
from pathlib import Path
from lxml import etree as ET
HERE=Path(__file__).resolve().parent;TASK=HERE.parent.parent
PLAN=json.loads((HERE/'payload.json').read_text(encoding='utf-8'))
NS='http://schemas.openxmlformats.org/spreadsheetml/2006/main';N={'x':NS}
def tag(x):return '{'+NS+'}'+x
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rn(c):return int(re.search(r'\d+',c)[0])
def cn(c):return re.match('[A-Z]+',c)[0]
def ci(c):
 n=0
 for a in c:n=n*26+ord(a)-64
 return n
def txt(c,shared):
 if c is None:return None
 if c.get('t')=='s':return shared[int(c.findtext('x:v',namespaces=N))]
 if c.get('t')=='inlineStr':return ''.join(c.find('x:is',N).itertext())
 return c.findtext('x:v',namespaces=N)
def readzip(z,member):return ET.fromstring(z.read(member))
def values(z,member):
 ss=[]
 if 'xl/sharedStrings.xml'in z.namelist():ss=[''.join(e.itertext())for e in readzip(z,'xl/sharedStrings.xml')]
 return{c.get('r'):txt(c,ss)for c in readzip(z,member).findall('.//x:sheetData/x:row/x:c',N)}
def shiftref(ref,at,count):
 return re.sub(r'(\$?[A-Z]{1,3}\$?)(\d+)',lambda m:m[1]+str(int(m[2])+count if int(m[2])>=at else int(m[2])),ref)
def shiftrows(sheet,at,count):
 for row in sheet.findall('x:sheetData/x:row',N):
  old=int(row.get('r'))
  if old>=at:row.set('r',str(old+count))
  for c in row:
   c.set('r',shiftref(c.get('r'),at,count))
   f=c.find('x:f',N)
   if f is not None and f.text:f.text=shiftref(f.text,at,count)
 for el in sheet.iter():
  for attr in ['ref','sqref']:
   if el.get(attr):el.set(attr,shiftref(el.get(attr),at,count))
 for bk in sheet.findall('x:rowBreaks/x:brk',N):
  if int(bk.get('id'))>=at:bk.set('id',str(int(bk.get('id'))+count))
def cell_at(sheet,addr):
 return sheet.find(f"x:sheetData/x:row[@r='{rn(addr)}']/x:c[@r='{addr}']",N)
def ensure_row(sheet,rownum,attrs=None):
 sd=sheet.find('x:sheetData',N); row=sd.find(f"x:row[@r='{rownum}']",N)
 if row is None:
  row=ET.Element(tag('row'),**(attrs or{}));row.set('r',str(rownum));sd.append(row)
 return row
def ensure_cell(sheet,addr,style=None):
 row=ensure_row(sheet,rn(addr));c=row.find(f"x:c[@r='{addr}']",N)
 if c is None:c=ET.SubElement(row,tag('c'),r=addr)
 if style is not None:c.set('s',str(style))
 return c
class Styles:
 def __init__(self,root):
  self.root=root;self.fonts=root.find('x:fonts',N);self.xfs=root.find('x:cellXfs',N);self.borders=root.find('x:borders',N);self.cache={};self.thin=None
 def body(self,oldid,col,book,isnew=False):
  key=(oldid,col,book['font'],book.get('baseSize',11),isnew,col in book['centerCols'])
  if key in self.cache:return self.cache[key]
  xf=copy.deepcopy(self.xfs[int(oldid)]);f=copy.deepcopy(self.fonts[int(xf.get('fontId','0'))])
  for nm,val in [('name',book['font']),('sz',str(book.get('baseSize',11)))]:
   e=f.find('x:'+nm,N)
   if e is None:e=ET.SubElement(f,tag(nm))
   e.set('val',val)
  self.fonts.append(f);fid=len(self.fonts)-1;xf.set('fontId',str(fid));xf.set('applyFont','1')
  a=xf.find('x:alignment',N)
  if a is None:a=ET.SubElement(xf,tag('alignment'))
  a.set('wrapText','1');a.set('vertical','center');a.set('horizontal','center'if col in book['centerCols']else'left')
  xf.set('applyAlignment','1')
  if isnew:
   if self.thin is None:
    border=ET.Element(tag('border'))
    for side in ['left','right','top','bottom']:
     edge=ET.SubElement(border,tag(side),style='thin');ET.SubElement(edge,tag('color'),rgb='FF000000')
    ET.SubElement(border,tag('diagonal'));self.borders.append(border);self.thin=len(self.borders)-1
   xf.set('borderId',str(self.thin));xf.set('applyBorder','1')
  self.xfs.append(xf);sid=len(self.xfs)-1;self.cache[key]=sid
  return sid
 def font(self,sid):return self.fonts[int(self.xfs[int(sid)].get('fontId','0'))]
 def finish(self):
  for e in[self.fonts,self.xfs,self.borders]:e.set('count',str(len(e)))
def put(c,value,styles):
 for child in list(c):c.remove(child)
 c.set('t','inlineStr');isnode=ET.SubElement(c,tag('is'))
 if value is None:value=''
 pattern=r'\[(?:QCC|QCC/E8300) 자료 입력\]|\[E8300 보완\]'
 pos=0
 for m in re.finditer(pattern,value):
  if m.start()<pos:continue
  end=value.find('\n',m.start())
  if end<0:end=len(value)
  parts=[]
  if m.start()>pos:parts.append((value[pos:m.start()],False,''))
  parts.append((value[m.start():end],True,m[0]))
  for text,special,prefix in parts:
   r=ET.SubElement(isnode,tag('r'));p=ET.SubElement(r,tag('rPr'));font=styles.font(c.get('s','0'))
   for e in font:
    if ET.QName(e).localname in['name','sz','b','i','color','family','charset','scheme']:
     q=copy.deepcopy(e)
     if ET.QName(q).localname=='name':q.tag=tag('rFont')
     p.append(q)
   if special:
    for e in list(p):
     if ET.QName(e).localname in['color','sz','u']:p.remove(e)
    size=float(font.find('x:sz',N).get('val','11'))+2
    ET.SubElement(p,tag('sz'),val=str(size));ET.SubElement(p,tag('u'))
    color='7030A0'if'QCC/E8300'in prefix else'0070C0'if'E8300 보완'in prefix else'C00000'
    ET.SubElement(p,tag('color'),rgb='FF'+color)
   t=ET.SubElement(r,tag('t'));t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=text
  pos=end
 if pos==0:
  t=ET.SubElement(isnode,tag('t'));t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=value
 elif pos<len(value):
  r=ET.SubElement(isnode,tag('r'));t=ET.SubElement(r,tag('t'));t.set('{http://www.w3.org/XML/1998/namespace}space','preserve');t.text=value[pos:]
def mergeadd(sheet,ref):
 merges=sheet.find('x:mergeCells',N)
 if merges is None:
  merges=ET.Element(tag('mergeCells'))
  # mergeCells follows sheetData / protection / autofilter and precedes page margins.
  target=next((e for e in sheet if ET.QName(e).localname in['phoneticPr','conditionalFormatting','dataValidations','hyperlinks','printOptions','pageMargins','pageSetup','headerFooter','rowBreaks','colBreaks','drawing']),None)
  if target is None:sheet.append(merges)
  else:sheet.insert(sheet.index(target),merges)
 if not any(x.get('ref')==ref for x in merges):ET.SubElement(merges,tag('mergeCell'),ref=ref)
 merges.set('count',str(len(merges)))
def sortdata(sheet):
 sd=sheet.find('x:sheetData',N)
 for row in sd:
  for c in sorted(list(row),key=lambda c:ci(cn(c.get('r')))):row.remove(c);row.append(c)
 for row in sorted(list(sd),key=lambda r:int(r.get('r'))):sd.remove(row);sd.append(row)
def main():
 manifest=[]
 for book in PLAN['books']:
  if len(sys.argv)>1 and book['key']!=sys.argv[1]:continue
  target=Path(book['target']);before=sha(target)
  assert sha(book['original'])==book['originalHash'],'original changed'
  # Exactly the values exported by the Artifact authoring operation are integrated.
  with zipfile.ZipFile(HERE/(book['key']+'-artifact.xlsx'))as z:
   authored=values(z,'xl/worksheets/sheet1.xml');covervals=values(z,'xl/worksheets/sheet2.xml');auxvals=values(z,'xl/worksheets/sheet3.xml')if book['aux']else{};historyvals=values(z,'xl/worksheets/sheet4.xml')if book['history']else{}
   for cell,value in book['cells'].items():assert (authored.get(cell)or'')==value,(book['key'],cell)
   for cell,value in book['cover'].items():assert (covervals.get(cell)or'')==value,(book['key'],cell)
   for cell,value in book['aux'].items():assert (auxvals.get(cell)or'')==value,(book['key'],cell)
   authored={cell:authored.get(cell)or''for cell in book['cells']}
   covervals={cell:covervals.get(cell)or''for cell in book['cover']}
   auxvals={cell:auxvals.get(cell)or''for cell in book['aux']}
   for cell,value in book['history'].items():assert(historyvals.get(cell)or'')==value,(book['key'],cell)
   historyvals={cell:historyvals.get(cell)or''for cell in book['history']}
  baseline=HERE/(book['key']+'-native-baseline.xlsx')
  if not baseline.exists():baseline.write_bytes(target.read_bytes())
  with zipfile.ZipFile(baseline)as z:infos=z.infolist();data={x.filename:z.read(x.filename)for x in infos};oldvalues=values(z,'xl/worksheets/sheet2.xml')
  styles=Styles(ET.fromstring(data['xl/styles.xml']));sheet=ET.fromstring(data['xl/worksheets/sheet2.xml']);cv=ET.fromstring(data['xl/worksheets/sheet1.xml'])
  # Native template cells supply only style/border/alignment references.
  template=1686 if book['kind']=='unit'else 119 if book['kind']=='integration'else 280
  oldstyles={c:((cell_at(sheet,f'{c}{template}').get('s','0'))if cell_at(sheet,f'{c}{template}')is not None else'0')for c in book['widths']}
  if book['insertCount']:shiftrows(sheet,book['insertAt'],book['insertCount'])
  for row in book['newRows']:
   r=ensure_row(sheet,row);r.set('ht',str(book['heights'][str(row)]));r.set('customHeight','1')
   for c in book['widths']:
    ensure_cell(sheet,f'{c}{row}',styles.body(oldstyles[c],c,book,True))
  changes=[]
  for addr,value in authored.items():
   old=cell_at(sheet,addr);same=old is not None and oldvalues.get(addr)==value
   new=rn(addr)in book['newRows']
   sid=old.get('s','0')if old is not None else oldstyles.get(cn(addr),'0')
   if new:sid=styles.body(oldstyles.get(cn(addr),'0'),cn(addr),book,True)
   elif not same and any(k in value for k in['[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]']):sid=styles.body(sid,cn(addr),book,False)
   if not same or new:
    c=ensure_cell(sheet,addr,sid);put(c,value,styles);changes.append(addr)
   # Existing changed system conditions need height for longer pending observations.
   if book['kind']=='system'and not new and (oldvalues.get(addr)or'').strip()!=value.strip()and rn(addr)>=34:
    row=ensure_row(sheet,rn(addr));row.set('ht','340');row.set('customHeight','1')
  for ref in book['newMerges']:mergeadd(sheet,ref)
  if book['kind']=='system':
   for rr in [78,80,82,83,184,185,190,199,200,201,202,206,207,208,209,210,211,212,213,214,222,223,224,227,228,229]:
    row=ensure_row(sheet,rr);row.set('ht','340');row.set('customHeight','1')
  for addr,value in covervals.items():
   c=cell_at(cv,addr)
   if c is None:
    tc=cell_at(cv,f'{cn(addr)}24');sid=tc.get('s','0')if tc is not None else'0';c=ensure_cell(cv,addr,sid)
   put(c,value,styles)
  for ref in list(cv.findall('x:mergeCells/x:mergeCell',N)):
   if re.search(r'\D24$',ref.get('ref'))and all(rn(x)==24 for x in ref.get('ref').split(':')):
    mergeadd(cv,re.sub('24','25',ref.get('ref')))
  ensure_row(cv,25).set('ht','70');ensure_row(cv,25).set('customHeight','1')
  sortdata(cv);sortdata(sheet)
  if book['kind']=='integration':
   sheet.find('x:dimension',N).set('ref',f'A1:{book["lastCol"]}{book["finalEnd"]}')
  wb=ET.fromstring(data['xl/workbook.xml'])
  for dn in wb.findall('x:definedNames/x:definedName',N):
   if dn.get('name')=='_xlnm.Print_Area'and dn.get('localSheetId')=='1':
    dn.text="'"+book['sheet'].replace("'","''")+f"'!$B$2:${book['lastCol']}${book['finalEnd']}"
  notices=[]
  for addr,value in book['cells'].items():
   for line in value.split('\n'):
    if re.search(r'\[(?:QCC|QCC/E8300) 자료 입력\]|\[E8300 보완\]',line):notices.append({'sheet':book['sheet'],'cell':addr,'text':line,'font_size':13,'underline':True})
  if book['aux']:
   # Workbook's last native sheet is the preserved cybersecurity link sheet.
   names=wb.find('x:sheets',N);lastidx=len(names)
   part=f'xl/worksheets/sheet{lastidx}.xml';ax=ET.fromstring(data[part])
   auxbook={**book,'font':'맑은 고딕','baseSize':10,'centerCols':['A','B','F','L']}
   for addr,value in auxvals.items():
    c=cell_at(ax,addr)
    sid=c.get('s','0')if c is not None else'0'
    if any(t in value for t in['[QCC 자료 입력]','[QCC/E8300 자료 입력]','[E8300 보완]']):sid=styles.body(sid,cn(addr),auxbook)
    c=ensure_cell(ax,addr,sid);put(c,value,styles)
    if rn(addr)>=5:
     row=ensure_row(ax,rn(addr));row.set('ht',str(min(409.5,max(float(row.get('ht','300')),340))));row.set('customHeight','1')
    if'자료 입력]'in value or'[E8300 보완]'in value:notices.append({'sheet':names[-1].get('name'),'cell':addr,'text':value,'font_size':12,'underline':True})
   sortdata(ax);data[part]=ET.tostring(ax,xml_declaration=True,encoding='UTF-8',standalone=True)
  if book['history']:
   hs=ET.fromstring(data['xl/worksheets/sheet3.xml']);hb={**book,'centerCols':['A','D']}
   for addr,value in historyvals.items():
    c=cell_at(hs,addr);sid=c.get('s','0')if c is not None else'0'
    sid=styles.body(sid,cn(addr),hb);c=ensure_cell(hs,addr,sid);put(c,value,styles)
    row=ensure_row(hs,rn(addr));row.set('ht','90'if rn(addr)==2 else'340');row.set('customHeight','1')
   mergeadd(hs,'A2:D2');sortdata(hs)
   data['xl/worksheets/sheet3.xml']=ET.tostring(hs,xml_declaration=True,encoding='UTF-8',standalone=True)
  styles.finish()
  data['xl/styles.xml']=ET.tostring(styles.root,xml_declaration=True,encoding='UTF-8',standalone=True)
  data['xl/worksheets/sheet1.xml']=ET.tostring(cv,xml_declaration=True,encoding='UTF-8',standalone=True)
  data['xl/worksheets/sheet2.xml']=ET.tostring(sheet,xml_declaration=True,encoding='UTF-8',standalone=True)
  data['xl/workbook.xml']=ET.tostring(wb,xml_declaration=True,encoding='UTF-8',standalone=True)
  temp=target.with_name(target.name+'.qcc-test-building')
  with zipfile.ZipFile(temp,'w',compression=zipfile.ZIP_DEFLATED)as z:
   for info in infos:z.writestr(info,data[info.filename])
  os.replace(temp,target)
  manifest.append({'key':book['key'],'path':str(target),'sheet':book['sheet'],'sheets':[e.get('name')for e in wb.find('x:sheets',N)],'sha256_before':before,'sha256_after':sha(target),'original_sha256_after':sha(book['original']),'changed_cells':changes,'history_changed_cells':list(historyvals),'added_rows':book['newRows'],'new_condition_rows':[min(book['newRows']),max(book['newRows'])],'groups':book['groups'],'inserted_rows':book['insertCount'],'common_columns':'B:'+book['commonLast'],'result_column':book['resultCol'],'verdict_column':book['verdictCol'],'missing_notices':notices,'preserved_package_members':[n for n in data if n.startswith('xl/media/')or n.startswith('xl/drawings/')],'authored_by':'Artifact Tool; selectively preserved original OOXML package'})
  print(json.dumps({'saved':book['key'],'hash':manifest[-1]['sha256_after'],'newrows':len(book['newRows'])},ensure_ascii=True))
 evidence=TASK/'evidence/qcc_tests_authored.json'
 if len(sys.argv)>1 and evidence.exists():
  previous=json.loads(evidence.read_text(encoding='utf-8'))
  prior={f['key']:f for f in previous['files']};prior.update({f['key']:f for f in manifest});manifest=list(prior.values())
 evidence.write_text(json.dumps({'owner':'qcc_tests','date':'2026-10-12','phase':'authored_pending_native_QA','files':manifest,'limitations':['QCC inputs are criteria, not executed test results.','Timing start/reset/tolerance, L/R encoding, shutdown signal, charge conversion and cryptographic vectors remain marked gaps.'],'verification':None},ensure_ascii=False,indent=2),encoding='utf-8')
if __name__=='__main__':main()
