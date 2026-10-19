from pathlib import Path
from lxml import etree as E
from PIL import ImageFont
import json,re,zipfile,hashlib,os,copy,math
HERE=Path(__file__).resolve().parent
P=json.loads((HERE/'author-payload.json').read_text(encoding='utf-8'))
NS='http://schemas.openxmlformats.org/spreadsheetml/2006/main';N={'x':NS}
RNS='http://schemas.openxmlformats.org/officeDocument/2006/relationships';XML='http://www.w3.org/XML/1998/namespace'
def tag(s):return '{'+NS+'}'+s
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def texts(root):return ''.join(root.xpath('.//x:t/text()',namespaces=N))
def sheetmap(data):
 rel=E.fromstring(data['xl/_rels/workbook.xml.rels']);links={r.get('Id'):r.get('Target') for r in rel}
 w=E.fromstring(data['xl/workbook.xml'])
 return {s.get('name'):('xl/'+links[s.get('{'+RNS+'}id')].lstrip('/') if not links[s.get('{'+RNS+'}id')].startswith('/xl/') else links[s.get('{'+RNS+'}id')][1:]) for s in w.find('x:sheets',N)}
def vals(data,member):
 ss=[]
 if 'xl/sharedStrings.xml' in data:ss=[texts(si) for si in E.fromstring(data['xl/sharedStrings.xml'])]
 out={}
 for c in E.fromstring(data[member]).findall('.//x:sheetData/x:row/x:c',N):
  if c.get('t')=='s':v=ss[int(c.findtext('x:v',namespaces=N))]
  elif c.get('t')=='inlineStr':v=texts(c)
  else:v=c.findtext('x:v',namespaces=N) or ''
  out[c.get('r')]=v
 return out
def ci(col):
 n=0
 for x in col:n=n*26+ord(x)-64
 return n
def ref(addr):m=re.fullmatch(r'([A-Z]+)(\d+)',addr);return ci(m[1]),int(m[2])
FONT={s:ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',round(s*96/72)) for s in [11,13]}
def required_height(value,width):
 px=max(30,width*7-8);height=0
 for line in value.splitlines():
  size=13 if re.search(r'\[(QCC|QCC/E8300) 자료 입력\]|\[E8300 보완\]',line) else 11
  f=FONT[size];used=0;count=1
  for ch in line:
   w=f.getlength(ch)
   if used+w>px:count+=1;used=0
   used+=w
  height+=count*(size*1.24)
 return height+8
def rich(c,comp,font):
 for child in list(c):c.remove(child)
 c.set('t','inlineStr');isn=E.SubElement(c,tag('is'))
 def run(t,color=None,special=False):
  if not t:return
  r=E.SubElement(isn,tag('r'));p=E.SubElement(r,tag('rPr'))
  for child in font:
   if E.QName(child).localname in ['name','family','charset','b','i','sz','color','scheme']:
    q=copy.deepcopy(child)
    if E.QName(q).localname=='name':q.tag=tag('rFont')
    if E.QName(q).localname not in ['sz','color'] or (not color and not special):p.append(q)
  if color:E.SubElement(p,tag('color'),rgb='FF'+color)
  if special:
   base=float(font.find('x:sz',N).get('val','11'));E.SubElement(p,tag('sz'),val=str(base+2));E.SubElement(p,tag('u'))
  elif color:E.SubElement(p,tag('sz'),val=font.find('x:sz',N).get('val','11'))
  tt=E.SubElement(r,tag('t'));tt.set('{'+XML+'}space','preserve');tt.text=t
 run('\n')
 for line in comp['lines']:
  status=line['status'];value=line['text'];color='C00000' if status=='old' else '008000' if status=='new' else None
  marker='[-] ' if status=='old' else '[+] ' if status=='new' else ''
  if marker:run(marker,color)
  match=re.search(r'\[(QCC|QCC/E8300) 자료 입력\]|\[E8300 보완\]',value)
  if match:
   if match.start():run(value[:match.start()],color)
   noticecolor='C00000' if status=='old' else '7030A0' if match[1]=='QCC/E8300' else '0070C0' if 'E8300' in match[0] else 'C00000'
   run(value[match.start():],noticecolor,True)
  else:run(value,color)
  run('\n',color)
 assert texts(c)==comp['display']
def main(write=False):
 manifest=[];splits=[]
 for k,b in P['books'].items():
  path=Path(b['path']);before=sha(path);assert before==b['expected_hash'],(k,'concurrent change')
  with zipfile.ZipFile(path) as z:infos=z.infolist();data={i.filename:z.read(i.filename) for i in infos}
  beforedata=data.copy();mapping=sheetmap(data)
  with zipfile.ZipFile(HERE/(k+'-authored.xlsx')) as z:stage={i.filename:z.read(i.filename) for i in z.infolist()}
  stagemap=sheetmap(stage);styles=E.fromstring(data['xl/styles.xml']);fonts=styles.find('x:fonts',N);xfs=styles.find('x:cellXfs',N)
  changed=[];heights={}
  for sn,cells in b['comparison'].items():
   member=mapping[sn];root=E.fromstring(data[member]);sv=vals(stage,stagemap[sn]);existing=vals(data,member)
   rows={int(r.get('r')):r for r in root.find('x:sheetData',N)}
   colwidth={}
   for col in root.findall('x:cols/x:col',N):
    for n in range(int(col.get('min')),int(col.get('max'))+1):colwidth[n]=float(col.get('width','13'))
   merges=[m.get('ref') for m in root.findall('x:mergeCells/x:mergeCell',N)]
   for addr,comp in cells.items():
    assert existing.get(addr,'')==comp['before'],(k,sn,addr,'before')
    assert sv.get(addr)==comp['display'],(k,sn,addr,'artifact authored')
    c=root.find(f".//x:sheetData/x:row/x:c[@r='{addr}']",N);assert c is not None,(k,sn,addr)
    font=fonts[int(xfs[int(c.get('s','0'))].get('fontId','0'))]
    rich(c,comp,font)
    cn,rn=ref(addr);merge=next((m for m in merges if m.split(':')[0]==addr),None)
    endcol,endrow=ref(merge.split(':')[-1]) if merge else (cn,rn)
    available=sum(float(rows[r].get('ht','15')) for r in range(rn,endrow+1))
    needed=required_height(comp['display'],sum(colwidth.get(n,13) for n in range(cn,endcol+1)))
    if needed>available:
     if endrow==rn:
      if needed>409.5:splits.append({'key':k,'sheet':sn,'cell':addr,'needed':round(needed,1),'available':available})
      else:heights.setdefault(sn,{})[rn]=max(heights.get(sn,{}).get(rn,available),math.ceil(needed))
     elif needed>409.5*(endrow-rn+1):splits.append({'key':k,'sheet':sn,'cell':addr,'needed':round(needed,1),'available':available})
     else:
      per=min(409.5,math.ceil(needed/(endrow-rn+1)))
      for r in range(rn,endrow+1):heights.setdefault(sn,{})[r]=max(heights.get(sn,{}).get(r,float(rows[r].get('ht','15'))),per)
   for r,h in heights.get(sn,{}).items():rows[r].set('ht',str(min(h,409.5)));rows[r].set('customHeight','1')
   data[member]=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
   changed.append(member)
  # Both members of a pair later receive the max of the two height proposals.
  manifest.append({'key':k,'path':str(path),'hash_before':before,'modified_members':changed,'heights':heights,'cells':{sn:list(c) for sn,c in b['comparison'].items()},'sheet_map':mapping})
  if write:
   assert not splits,'Run preflight and split before writing'
   temp=path.with_name(path.name+'.qccreview')
   with zipfile.ZipFile(temp,'w') as z:
    for info in infos:z.writestr(info,data[info.filename])
   os.replace(temp,path)
   assert all(data[m]==beforedata[m] for m in data if m not in changed)
   manifest[-1]['hash_after']=sha(path)
 (HERE/('integrated.json' if write else 'preflight.json')).write_text(json.dumps({'files':manifest,'split_candidates':splits},ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps({'write':write,'split_candidates':splits,'height_changes':{x['key']:x['heights'] for x in manifest}},ensure_ascii=True))
if __name__=='__main__':
 import sys;main('--write' in sys.argv)
