"""Verify body change semantics before publishing an Office field-refresh candidate."""
from pathlib import Path
from zipfile import ZipFile
from collections import defaultdict
from lxml import etree as E
import argparse,hashlib,json,posixpath,sys
sys.stdout.reconfigure(encoding='utf-8')
p=argparse.ArgumentParser();p.add_argument('before');p.add_argument('after');p.add_argument('record');a=p.parse_args()
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';R='http://schemas.openxmlformats.org/officeDocument/2006/relationships';N={'w':W,'r':R}
def sha(data):return hashlib.sha256(data).hexdigest()
def inspect(path):
 with ZipFile(path) as z:
  doc=E.fromstring(z.read('word/document.xml'));body=doc.find('w:body',N)
  for sdt in list(body.findall('w:sdt',N)):
   if sdt.find('.//w:docPartGallery',N) is not None:body.remove(sdt)
  targets={r.get('Id'):posixpath.normpath('word/'+r.get('Target','')) for r in E.fromstring(z.read('word/_rels/document.xml.rels'))}
  def semantic(mode):
   parts=[];pictures=[]
   def walk(node):
    tag=E.QName(node).localname
    if node.tag==f'{{{W}}}del' and mode=='accept':return
    if node.tag==f'{{{W}}}ins' and mode=='reject':return
    if node.tag==f'{{{W}}}tr':
     if mode=='accept' and node.find('w:trPr/w:del',N) is not None:return
     if mode=='reject' and node.find('w:trPr/w:ins',N) is not None:return
    if node.tag==f'{{{W}}}fldSimple':return
    # Use field membership from the complete XML tree. Projecting revisions
    # before pairing field begin/end can leak SEQ results from historical
    # caption revisions into a reject view after Word normalizes the fields.
    if node in ordinary_text:parts.append(node.text or '')
    if node.tag==f'{{{W}}}drawing':
     for ref in node.xpath('.//@r:embed',namespaces=N):pictures.append(sha(z.read(targets[ref])))
    for child in node:walk(child)
   walk(body)
   return {'body_text_sha256':sha(''.join(parts).encode()),'body_text_length':sum(map(len,parts)),'picture_hashes_in_order':pictures}
  # Word may move a SEQ/REF result out of a revision when refreshing it.
  # Compare revision history with the same computed-field exclusion as the
  # accepted/rejected body checks; ordinary inserted/deleted text stays strict.
  ordinary_text=set();field_depth=0
  def collect_ordinary(node):
   nonlocal field_depth
   if node.tag==f'{{{W}}}fldSimple':return
   if node.tag==f'{{{W}}}fldChar':
    kind=node.get(f'{{{W}}}fldCharType')
    if kind=='begin':field_depth+=1
    elif kind=='end':field_depth=max(0,field_depth-1)
   if node.tag in (f'{{{W}}}t',f'{{{W}}}delText') and field_depth==0:ordinary_text.add(node)
   for child in node:collect_ordinary(child)
  collect_ordinary(body)
  revisions=defaultdict(list)
  for rev in body.xpath('.//w:ins | .//w:del',namespaces=N):
   text=''.join(t.text or '' for t in rev.iter() if t in ordinary_text)
   if not text:continue # Drawings verified by accepted/rejected picture hashes.
   key=(E.QName(rev).localname,rev.get(f'{{{W}}}author'),(rev.get(f'{{{W}}}date') or '')[:16])
   revisions[key].append(text)
  fingerprints={'|'.join(k):{'length':len(''.join(v)),'sha256':sha(''.join(v).encode())}for k,v in revisions.items()}
  notices=[]
  for para in body.findall('.//w:p',N):
   runs=[r for r in para.findall('.//w:r',N) if not any(e.tag==f'{{{W}}}del' or(e.tag==f'{{{W}}}tr' and e.find('w:trPr/w:del',N)is not None)for e in r.iterancestors())]
   text=''.join(''.join(t.text or ''for t in r.findall('w:t',N))for r in runs)
   if any(t in text for t in ('[QCC 자료 입력]','[E8300 보완]','[QCC/E8300 자료 입력]')):
    fmt=[]
    for r in runs:
     rp=r.find('w:rPr',N)
     fmt.append({k:(rp.find('w:'+k,N).get(f'{{{W}}}val')if rp is not None and rp.find('w:'+k,N)is not None else None)for k in ('color','sz','u','b')})
    notices.append({'text':text,'run_format':fmt})
  return {'accept':semantic('accept'),'reject':semantic('reject'),'body_revision_fingerprints':fingerprints,'current_notice_text':[n['text']for n in notices]}
b,c=inspect(a.before),inspect(a.after)
checks={k:b[k]==c[k]for k in b}
result={'before':a.before,'after':a.after,'before_sha256':sha(Path(a.before).read_bytes()),'after_sha256':sha(Path(a.after).read_bytes()),'checks':checks,'before_semantics':b,'after_semantics':c,'pass':all(checks.values()),'scope':'TOC and computed fields excluded; accepted/rejected body text, drawings and author/date revision text preserved'}
Path(a.record).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'pass':result['pass'],'checks':checks,'record':a.record},ensure_ascii=False))
if not result['pass']:sys.exit(1)
