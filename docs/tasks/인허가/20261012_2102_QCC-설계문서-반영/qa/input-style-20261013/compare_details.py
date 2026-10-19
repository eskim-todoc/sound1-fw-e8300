from build_inputs import *
import difflib
sys.stdout.reconfigure(encoding='utf-8')
for code in ['SR','SGR']:
 seq=[]
 for suffix in ['candidate','fields']:
  d=E.fromstring(readzip(Q/(code+'-'+suffix+'.docx'))['word/document.xml']);body=d.find(qn('body'))
  for s in body.findall(qn('sdt')):body.remove(s)
  texts=[txt(p) for p in body.xpath('.//w:p',namespaces=N)]
  seq.append([t for t in texts if t])
 print(code)
 for l in difflib.unified_diff(*seq,lineterm='',n=1):print(l)
 before=json.loads((Q/(code+'-field-compare.json')).read_text(encoding='utf-8'))
 for k,v in before['before_semantics']['body_revision_fingerprints'].items():
  after=before['after_semantics']['body_revision_fingerprints'].get(k)
  if after!=v:print('revision fingerprint change',k,v,after)
