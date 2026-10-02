"""Verify current content, reject restoration, numbering and protected parts."""
from pathlib import Path
import json,re,zipfile,hashlib,argparse
from lxml import etree as E
T=Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--kind',choices=['sip','sad'],default='sip');args=parser.parse_args()
Q=T/('qa/'+args.kind+'-common-style')
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';q=lambda t:'{'+W+'}'+t;N={'w':W}
def read(p):
 with zipfile.ZipFile(p) as z:return {n:z.read(n) for n in z.namelist()}
def root(p):return E.fromstring(read(p)['word/document.xml'])
def strings(r):
 # TOC page caches are generated output, not normative document content.
 return ''.join(r.xpath('./w:body/*[not(self::w:sdt)]//w:t[not(ancestor::w:del)]/text()',namespaces=N))
def native(kind):return json.loads((Q/(kind+'-headings.json')).read_text(encoding='utf-8-sig'))
old=read(Q/'before.docx');new=read(Q/'candidate.docx');d=E.fromstring(new['word/document.xml']);b=E.fromstring(old['word/document.xml'])
assert strings(root(Q/'before-accepted.docx'))==strings(root(Q/'candidate-accepted.docx')),'Accepted content changed'
assert strings(root(Q/'before-rejected.docx'))==strings(root(Q/'candidate-rejected.docx')),'Rejected content changed'
before_h=native('before');after_h=native('candidate')
assert [(h['text'],h['number']) for h in before_h]==[(h['text'],h['number']) for h in after_h],'Native heading numbering changed'
for name,data in old.items():
 if name not in ['word/document.xml','word/styles.xml','word/numbering.xml','word/settings.xml']:assert new[name]==data,name
for tag in ['ins','del','pPrChange','rPrChange','tblPrChange','tcPrChange','trPrChange']:
 found={x.get(q('id')):x for x in d.findall('.//'+q(tag))}
 for old_node in b.findall('.//'+q(tag)):
  key=old_node.get(q('id'));assert key in found,(tag,key)
  assert old_node.get(q('author'))==found[key].get(q('author'))
assert E.fromstring(new['word/settings.xml']).find(q('trackRevisions')) is not None
st=E.fromstring(new['word/styles.xml']);defs={s.get(q('styleId')):s for s in st.findall(q('style'))}
custom={s.find(q('name')).get(q('val')):s for s in defs.values() if s.find(q('name')).get(q('val')).startswith('TDC ')}
assert len(custom)==59
for level,size in enumerate([14,13,12,11,10,10],1):
 s=custom['TDC 제목 '+str(level)];assert s.find('./'+q('rPr')+'/'+q('sz')).get(q('val'))==str(size*2)
 s=custom['TDC 본문 '+str(level)];assert s.find('./'+q('rPr')+'/'+q('sz')).get(q('val'))=='20'
started=False;body_paragraphs=0;red=[];tables=0
for block in d.find(q('body')):
 t=''.join(block.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=N))
 if t=='서론':started=True
 if not started:continue
 if block.tag==q('p'):
  if block.xpath('./w:pPr/w:rPr/w:del',namespaces=N) or (not t and block.xpath('.//w:delText',namespaces=N)):continue
  ps=block.find('./'+q('pPr')+'/'+q('pStyle'));assert ps is not None and ps.get(q('val')) in defs
  assert defs[ps.get(q('val'))].find(q('name')).get(q('val')).startswith('TDC '),t
  body_paragraphs+=1
  for sz in block.xpath('.//w:sz[not(ancestor::w:pPrChange) and not(ancestor::w:rPrChange) and not(ancestor::w:del)]/@w:val',namespaces=N):assert int(sz)>=18,(t,sz)
  if '[자료 추가 위치]' in t:
   assert block.xpath('.//w:color[not(ancestor::w:pPrChange) and not(ancestor::w:rPrChange)]/@w:val',namespaces=N),t
   red.append(t)
 elif block.tag==q('tbl'):
  tables+=1
  # Spacing after the table remains a visible ordinary-size body paragraph.
  p=block.getnext();assert p is not None and p.tag==q('p')
  assert not ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=N)), 'Missing table spacer'
  assert not p.xpath('./w:pPr/w:rPr/w:del',namespaces=N),'Deleted table spacer'
assert len(red)==4 if args.kind=='sip' else not red
if args.kind=='sad':
 cells=root(Q/'candidate-accepted.docx').xpath('./w:body/w:tbl/w:tr/w:tc',namespaces=N)
 ids=[m[1] for c in cells if (m:=re.fullmatch(r'SW-FW-RID-(\d+)',''.join(c.xpath('.//w:t/text()',namespaces=N))))]
 assert set(map(int,ids))==set(range(1,82)), 'Missing requirement IDs'
report={'content_unchanged':True,'rejection_restores_before_content':True,'heading_numbers_unchanged':len(after_h),'styles_embedded':len(custom),'body_paragraphs_styled':body_paragraphs,'body_tables':tables,'protected_parts_unchanged':True,'prior_revisions_preserved':True,'qcc_red_markers':len(red),'candidate_sha256':hashlib.sha256((Q/'candidate.docx').read_bytes()).hexdigest()}
(Q/'verify.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps(report,ensure_ascii=False))
