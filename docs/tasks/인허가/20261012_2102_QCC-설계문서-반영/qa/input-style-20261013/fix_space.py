from build_inputs import *
p=Q/'SR-candidate.docx';parts=readzip(p);doc=E.fromstring(parts['word/document.xml']);found=doc.xpath('//w:t[starts-with(text()," IQS323")]',namespaces=N);assert len(found)==1
found[0].set('{http://www.w3.org/XML/1998/namespace}space','preserve');parts['word/document.xml']=xml(doc);writezip(p,parts)
a=json.loads((Q/'authored.json').read_text(encoding='utf-8'))
for item in a['docs']:
 if item['code']=='SR':
  item['candidate_sha256']=sha(p);item['split_run_whitespace_preserved']=True
  (Q/'SR-authored.json').write_text(json.dumps(item,ensure_ascii=False,indent=2),encoding='utf-8')
(Q/'authored.json').write_text(json.dumps(a,ensure_ascii=False,indent=2),encoding='utf-8');print(sha(p))
