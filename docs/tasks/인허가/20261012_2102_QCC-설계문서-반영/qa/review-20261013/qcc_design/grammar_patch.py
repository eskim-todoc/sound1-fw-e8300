from pathlib import Path
from copy import deepcopy
from datetime import datetime,timezone
import zipfile,json,hashlib
from lxml import etree as E
BASE=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기')
ROOT=Path(__file__).parent/'grammar';ROOT.mkdir(exist_ok=True)
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';NS={'w':W};Q=lambda t:'{'+W+'}'+t
DATE=datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00','Z')
REPLACE={'장치명를':'장치명을','활성를':'활성화를','하향를':'하향 경로를'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def visible(x):
 if x.tag==Q('del')or(x.tag==Q('tr')and x.find('w:trPr/w:del',NS)is not None):return ''
 if x.tag==Q('t'):return x.text or ''
 if x.tag==Q('br'):return '\n'
 return ''.join(visible(c)for c in x)
def main():
 result={}
 for code in ['SAD','SDDD']:
  src=next(BASE.glob('[[]E8300-QCC[]] '+code+'*.docx'));beforehash=sha(src)
  baseline=ROOT/(code+'-before.docx');baseline.write_bytes(src.read_bytes())
  with zipfile.ZipFile(src)as z:entries=[(deepcopy(i),z.read(i.filename))for i in z.infolist()]
  parts={i.filename:b for i,b in entries};r=E.fromstring(parts['word/document.xml']);before=deepcopy(r);beforevisible=visible(r)
  nextid=max(int(v)for v in r.xpath('.//@w:id',namespaces=NS)if v.isdigit())+1;ids=set();changes=[]
  def rev(kind):
   nonlocal nextid
   el=E.Element(Q(kind));el.set(Q('id'),str(nextid));el.set(Q('author'),'김은수');el.set(Q('date'),DATE);ids.add(str(nextid));nextid+=1;return el
  containers=[]
  for t in r.xpath('.//w:t[not(ancestor::w:del)]',namespaces=NS):
   if not any(x in(t.text or '')for x in REPLACE):continue
   parent=t.getparent().getparent();assert parent.tag==Q('ins')and parent.getparent().tag==Q('p')
   if parent not in containers:containers.append(parent)
  for old in containers:
   assert all(c.tag==Q('r')for c in old),'unexpected non-run active insertion'
   p=old.getparent();oldtext=visible(old);new=rev('ins')
   for child in old:new.append(deepcopy(child))
   hits={}
   for t in new.iter(Q('t')):
    for a,b in REPLACE.items():
     if a in(t.text or ''):hits[a]=hits.get(a,0)+t.text.count(a);t.text=t.text.replace(a,b)
   deletion=rev('del')
   for child in list(old):
    old.remove(child);deletion.append(child)
    for t in child.iter(Q('t')):t.tag=Q('delText')
   old.append(deletion);p.insert(p.index(old)+1,new)
   changes.append({'para_id':p.get('{http://schemas.microsoft.com/office/word/2010/wordml}paraId'),'before':oldtext,'after':visible(new),'replacements':hits,'old_insertion_id':old.get(Q('id')),'new_insertion_id':new.get(Q('id')),'new_deletion_id':deletion.get(Q('id')),'pPr_preserved':True})
  # Reject ONLY this patch; the exact immediate baseline XML must return.
  rejected=deepcopy(r)
  for el in list(rejected.xpath('.//*[@w:id]',namespaces=NS))[::-1]:
   if el.get(Q('id'))not in ids:continue
   parent=el.getparent();index=parent.index(el)
   if el.tag==Q('del'):
    for c in list(el):
     for t in c.iter(Q('delText')):t.tag=Q('t')
     el.remove(c);parent.insert(index,c);index+=1
   parent.remove(el)
  assert E.tostring(rejected,method='c14n')==E.tostring(before,method='c14n'),'reject baseline mismatch'
  expected=beforevisible
  for a,b in REPLACE.items():expected=expected.replace(a,b)
  assert visible(r)==expected,'unexpected accepted text difference'
  assert not any(x in visible(r)for x in REPLACE)
  assert not r.xpath('.//w:ins[@w:id="'+next(iter(ids))+'"]/ancestor::w:ins',namespaces=NS)
  for el in r.xpath('.//w:ins',namespaces=NS):
   if el.get(Q('id'))in ids:assert not el.xpath('ancestor::w:ins|ancestor::w:del',namespaces=NS)
  parts['word/document.xml']=E.tostring(r,encoding='UTF-8',xml_declaration=True,standalone=True)
  candidate=ROOT/src.name
  with zipfile.ZipFile(candidate,'w')as z:
   for info,data in entries:z.writestr(info,parts.get(info.filename,data))
  assert sha(src)==beforehash,'source concurrent change'
  result[code]={'source':str(src),'before_sha256':beforehash,'candidate':str(candidate),'candidate_sha256':sha(candidate),'status':'통과','changes':changes,'new_revision_ids':sorted(ids,key=int),'reject_exact_baseline_xml':True,'accepted_text_only_requested_grammar':True,'all_other_package_parts_exact':True,'styles_and_body_pPr_exact':True,'G_published':False}
 (ROOT/'proof.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
 print(json.dumps({k:{'candidate':v['candidate'],'hash':v['candidate_sha256'],'paragraphs':len(v['changes'])}for k,v in result.items()},ensure_ascii=True))
if __name__=='__main__':main()
