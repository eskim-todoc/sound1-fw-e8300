import copy,json,pathlib,zipfile,re,hashlib,math,sys
from lxml import etree as E
ROOT=pathlib.Path(__file__).parent
P=json.loads((ROOT/'requirements-contract.json').read_text(encoding='utf-8'))
D=json.loads((ROOT/'inspection.json').read_text(encoding='utf-8'))
TARGET=pathlib.Path(D['files']['[E8300-QCC] SRS']['path'])
X='http://schemas.openxmlformats.org/spreadsheetml/2006/main';q=lambda t:'{'+X+'}'+t
NS={'x':X};XML='{http://www.w3.org/XML/1998/namespace}space'
COLORS={'[QCC 자료 입력]':'C00000','[QCC/E8300 자료 입력]':'7030A0','[E8300 보완]':'0070C0'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def rz(p):
 with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
def wz(p,parts):
 with zipfile.ZipFile(p,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in parts.items():z.writestr(n,b)
def xml(e):return E.tostring(e,encoding='UTF-8',xml_declaration=True,standalone=True)
def weight(s):return sum(1 if ord(c)>255 else .5 for c in s)
def split(text,limit=430):
 lines=text.splitlines(keepends=True);chunks=[];cur='';w=0
 for line in lines:
  lw=weight(line)+22
  if cur and w+lw>limit:chunks.append(cur);cur='';w=0
  cur+=line;w+=lw
 if cur:chunks.append(cur)
 return chunks or ['']
def prepare():
 assert sha(TARGET)==P['source_current_sha']
 parts=rz(TARGET);doc=E.fromstring(parts['xl/worksheets/sheet2.xml']);data=doc.find(q('sheetData'));styles=E.fromstring(parts['xl/styles.xml'])
 cols=doc.find(q('cols'));print('Fwidth',[n.attrib for n in cols if int(n.get('min'))<=6<=int(n.get('max'))])
 req={r['old_row']:r for r in P['requirements']};allrows=list(data);rowmap={};endmap={};newdata=[];edits=[];merges_new=[];rnow=0;blocks=[]
 for row in allrows:
  n=int(row.get('r'));rnow=max(rnow,n+sum(endmap[x]-rowmap[x] for x in rowmap)) if not rowmap else rnow+1
  # Original worksheet contains gaps; keep them before applying cumulative insertions.
  rnow=n+sum(endmap[x]-rowmap[x] for x in rowmap)
  r=req.get(n);cells={c.get('r'):c for c in row};oldrow=copy.deepcopy(row)
  values={}
  if n==49:
   current=D['files']['[E8300-QCC] SRS']['sheets'][1]['rows'];source=next(x for x in current if x['row']==49)
   values={col:'\n[-] '+(source['cells'].get(col+'49',{}).get('value') or '').strip('\n')+'\n' for col in 'FGHI'}
   active=None
  elif r:
   active=r['F'];values={}
   for col,new,old in [('F',r['F'],r['old_F']),('H',r['H'],r['old_H']),('I',r['I'],r['old_I'])]:
    if new!=old:
     values[col]='\n[-] '+old.strip('\n')+'\n[+] '+new.strip('\n')+'\n'
  else:active=None
  chunks=split(values.get('F','')) if values.get('F') else ['']
  count=len(chunks);rowmap[n]=rnow;endmap[n]=rnow+count-1
  for ix in range(count):
   rowcopy=copy.deepcopy(oldrow);rowcopy.set('r',str(rnow+ix));newcells={}
   for c in rowcopy:
    col=re.match('[A-Z]+',c.get('r'))[0];c.set('r',col+str(rnow+ix));newcells[col]=c
    if ix and col!='F':
     for ch in list(c):c.remove(ch)
     c.attrib.pop('t',None)
   if values:
    for col in values:
     if ix and col!='F':continue
     val=chunks[ix] if col=='F' else values[col]
     if col not in newcells:newcells[col]=E.SubElement(rowcopy,q('c'),r=col+str(rnow+ix),s='4')
     edits.append({'cell':col+str(rnow+ix),'value':val,'rid':r['rid'] if r else 82,'old_row':n,'column':col,'chunk':ix,'active_text':active if ix==0 and col=='F' else None})
    if ix or count>1:rowcopy.set('ht',str(max(20,float(oldrow.get('ht','20'))/count)));rowcopy.set('customHeight','1')
   newdata.append(rowcopy)
  if count>1:
   for col in 'GHI':merges_new.append(col+str(rnow)+':'+col+str(rnow+count-1))
  if r:blocks.append({'rid':r['rid'],'rows':[rnow,rnow+count-1],'F':r['F'],'H':r['H'],'I':r['I'],'matrix':r['matrix']})
 def adj(ref):
  def f(m):
   n=int(m[2]);return m[1]+str(rowmap.get(n,n+sum(endmap[x]-rowmap[x] for x in rowmap if x<n)))
  return re.sub(r'(\$?[A-Z]{1,3}\$?)(\d+)',f,ref)
 def merge_ref(ref):
  match=re.fullmatch(r'([A-Z]+)(\d+):([A-Z]+)(\d+)',ref)
  if not match:return adj(ref)
  c1,n1,c2,n2=match.groups();n1=int(n1);n2=int(n2)
  return c1+str(rowmap.get(n1,n1))+':'+c2+str(endmap.get(n2,n2))
 for child in list(data):data.remove(child)
 for row in newdata:data.append(row)
 for e in doc.iter():
  if e.tag==q('mergeCell'):e.set('ref',merge_ref(e.get('ref')))
  elif e.get('ref') and e.tag in [q('dimension'),q('autoFilter')]:e.set('ref',adj(e.get('ref')))
  if e.get('sqref'):e.set('sqref',adj(e.get('sqref')))
 merges=doc.find(q('mergeCells'))
 # Horizontal C:E classification must also cover the additional continuation rows.
 def bounds(ref):
  colsnums=[]
  for col,num in re.findall(r'([A-Z]+)(\d+)',ref):
   cnum=0
   for ch in col:cnum=cnum*26+ord(ch)-64
   colsnums.append((cnum,int(num)))
  return colsnums[0]+colsnums[-1]
 def overlap(a,b):
  a1,r1,a2,r2=bounds(a);b1,s1,b2,s2=bounds(b)
  return max(a1,b1)<=min(a2,b2) and max(r1,s1)<=min(r2,s2)
 for n in rowmap:
  if endmap[n]>rowmap[n]:
   candidate=f'C{rowmap[n]}:E{endmap[n]}'
   if not any(overlap(m.get('ref'),candidate) for m in merges):E.SubElement(merges,q('mergeCell'),ref=candidate)
 for ref in merges_new:E.SubElement(merges,q('mergeCell'),ref=ref)
 merges.set('count',str(len(merges)))
 parts['xl/worksheets/sheet2.xml']=xml(doc)
 wb=E.fromstring(parts['xl/workbook.xml'])
 for e in wb.findall('.//'+q('definedName')):
  if e.get('localSheetId')=='1' and e.text:e.text=adj(e.text)
 parts['xl/workbook.xml']=xml(wb)
 if 'xl/calcChain.xml' in parts:
  cc=E.fromstring(parts['xl/calcChain.xml']);sid=None
  for c in cc:
   sid=c.get('i',sid)
   if sid=='3':
    if c.get('r')=='G49':cc.remove(c)
    else:c.set('r',adj(c.get('r')))
  parts['xl/calcChain.xml']=xml(cc)
 if 'xl/comments1.xml' in parts:
  cm=E.fromstring(parts['xl/comments1.xml'])
  for c in cm.findall('.//'+q('comment')):c.set('ref',adj(c.get('ref')))
  parts['xl/comments1.xml']=xml(cm)
 if 'xl/drawings/vmlDrawing1.vml' in parts:
  vml=E.fromstring(parts['xl/drawings/vmlDrawing1.vml']);vx='{urn:schemas-microsoft-com:office:excel}'
  for e in vml.iter(vx+'Row'):e.text=str(rowmap.get(int(e.text)+1,int(e.text)+1)-1)
  for e in vml.iter(vx+'Anchor'):
   a=[int(x.strip()) for x in e.text.split(',')]
   for ix in [2,6]:a[ix]=rowmap.get(a[ix]+1,a[ix]+1)-1
   e.text=', '.join(map(str,a))
  parts['xl/drawings/vmlDrawing1.vml']=xml(vml)
 wz(ROOT/'srs-structural.xlsx',parts)
 (ROOT/'srs-cell-plan.json').write_text(json.dumps({'edits':edits,'blocks':blocks,'rowmap':rowmap,'endmap':endmap,'snapshot_sha':sha(TARGET)},ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps({'edits':len(edits),'split_requirements':[b for b in blocks if b['rows'][0]!=b['rows'][1]],'retired_row':rowmap[49]},ensure_ascii=False))
def finish():
 plan=json.loads((ROOT/'srs-cell-plan.json').read_text(encoding='utf-8'));parts=rz(ROOT/'srs-structural.xlsx');ap=rz(ROOT/'srs-artifact.xlsx');ad=E.fromstring(ap['xl/worksheets/sheet2.xml']);ass=[''.join(e.xpath('.//x:t/text()',namespaces=NS)) for e in E.fromstring(ap['xl/sharedStrings.xml'])] if 'xl/sharedStrings.xml' in ap else []
 artifact={}
 for c in ad.findall('.//'+q('c')):
  v=c.find(q('v'));artifact[c.get('r')]=''.join(c.xpath('.//x:t/text()',namespaces=NS)) if c.get('t')=='inlineStr' else ass[int(v.text)] if c.get('t')=='s' and v is not None else v.text if v is not None else ''
 for edit in plan['edits']:assert (artifact.get(edit['cell']) or '')==edit['value'],edit['cell']
 doc=E.fromstring(parts['xl/worksheets/sheet2.xml']);cells={c.get('r'):c for c in doc.findall('.//'+q('c'))};rows={int(r.get('r')):r for r in doc.find(q('sheetData'))};sty=E.fromstring(parts['xl/styles.xml']);fonts=sty.find(q('fonts'));xfs=sty.find(q('cellXfs'));borders=sty.find(q('borders'));cache={}
 def setstyle(c,col,ix,continuation):
  old=int(c.get('s','0'));key=(old,col,ix,continuation)
  if key not in cache:
   xf=copy.deepcopy(xfs[old]);a=xf.find(q('alignment'))
   if a is None:a=E.SubElement(xf,q('alignment'))
   a.set('wrapText','1');a.set('vertical','center');a.set('horizontal','left' if col=='F' else 'center');xf.set('applyAlignment','1')
   if continuation:
    bor=copy.deepcopy(borders[int(xf.get('borderId','0'))]);tag='top' if ix else 'bottom';br=bor.find(q(tag))
    if br is not None:
     br.attrib.clear()
     for ch in list(br):br.remove(ch)
    borders.append(bor);xf.set('borderId',str(len(borders)-1));xf.set('applyBorder','1')
   xfs.append(xf);cache[key]=len(xfs)-1
  c.set('s',str(cache[key]));return fonts[int(xfs[old].get('fontId','0'))]
 states={}
 for edit in plan['edits']:
  c=cells[edit['cell']];n=int(re.search(r'\d+$',edit['cell'])[0]);count=plan['endmap'][str(edit['old_row'])]-plan['rowmap'][str(edit['old_row'])]+1
  font=setstyle(c,edit['column'],edit['chunk'],count>1);sz=float(font.find(q('sz')).get('val','10'))
  for ch in list(c):c.remove(ch)
  c.set('t','inlineStr');si=E.SubElement(c,q('is'));state=states.get((edit['old_row'],edit['column']),'old')
  lines=edit['value'].splitlines(keepends=True)
  for line in lines:
   if line.lstrip().startswith('[+]'):state='new'
   if line.lstrip().startswith('[-]'):state='old'
   marker=next((p for p in COLORS if line.lstrip().startswith(p)),None) if state=='new' else None
   r=E.SubElement(si,q('r'));pr=E.SubElement(r,q('rPr'))
   for tag in ['name','b','i','family','charset','scheme']:
    f=font.find(q(tag))
    if f is not None:ch=copy.deepcopy(f);ch.tag=q('rFont') if tag=='name' else ch.tag;pr.append(ch)
   E.SubElement(pr,q('sz'),val=str(sz+2 if marker else sz))
   E.SubElement(pr,q('color'),rgb='FF'+(COLORS[marker] if marker else 'C00000' if state=='old' else '008000'))
   if marker:E.SubElement(pr,q('u'),val='single')
   t=E.SubElement(r,q('t'));t.set(XML,'preserve');t.text=line
  states[(edit['old_row'],edit['column'])]=state
  # Height for changed view, retaining source height where already sufficient.
  capacity=24 if edit['column']=='F' else 10
  linescount=sum(max(1,math.ceil(weight(line)/capacity)) for line in lines)
  needed=linescount*15+10
  rows[n].set('ht',str(min(409.5,max(float(rows[n].get('ht','20')),needed))));rows[n].set('customHeight','1')
 xfs.set('count',str(len(xfs)));borders.set('count',str(len(borders)));parts['xl/styles.xml']=xml(sty);parts['xl/worksheets/sheet2.xml']=xml(doc)
 assert sha(TARGET)==plan['snapshot_sha'],'Current SRS changed; abort'
 wz(TARGET,parts)
 plan['output_sha256']=sha(TARGET);plan['artifact_values_verified']=True
 (ROOT/'srs-authored.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps({'saved':str(TARGET),'sha256':sha(TARGET),'active_count':len(plan['blocks']),'changed_cells':len(plan['edits'])},ensure_ascii=False))
if __name__=='__main__':prepare() if sys.argv[1]=='prepare' else finish()
