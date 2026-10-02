from pathlib import Path
import json,re,zipfile
from lxml import etree as E
T=Path(__file__).resolve().parent
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';NS={'w':W};q=lambda x:'{'+W+'}'+x
P=T/'qa/design-readability/common-style/candidate-accepted.docx'
with zipfile.ZipFile(P) as z:doc=E.fromstring(z.read('word/document.xml'))
units=[];cur=None
for p in doc.find(q('body')):
    if p.tag!=q('p'):continue
    text=''.join(p.xpath('.//w:t/text()',namespaces=NS))
    m=re.match(r'SU-ID-(CFX|HEAR|CM3|BOOT)-(\d+), (.*)',text)
    if m:cur={'id':m.group(1)+'-'+m.group(2),'name':m.group(3),'fields':[]};units.append(cur);continue
    if cur and re.match('목적 :|입력 :|출력 :|처리 :|동작 조건 :|인터페이스|구현 위치 :|입출력 예 :',text):cur['fields'].append(text)
qa=T/'qa/design-readability/structured-units';qa.mkdir(exist_ok=True,parents=True)
(qa/'units-before.json').write_text(json.dumps(units,ensure_ascii=False,indent=2),encoding='utf8')
print('units',len(units))
for u in units:print(u['id'],sum(map(len,u['fields'])),len(u['fields']),u['name'])
print(json.dumps(units[:2],ensure_ascii=False,indent=2))
