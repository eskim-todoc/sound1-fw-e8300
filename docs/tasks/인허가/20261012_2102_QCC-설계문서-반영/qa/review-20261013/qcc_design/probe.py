from pathlib import Path
import json,zipfile
from copy import deepcopy
from lxml import etree as E
ROOT=Path(__file__).parent
TASK=ROOT.parents[2]
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';NS={'w':W}
q=lambda n:'{'+W+'}'+n
def text(x):
    if x.tag==q('tr') and x.find('w:trPr/w:del',NS) is not None:return ''
    if x.tag==q('del'):return ''
    if x.tag==q('t'):return x.text or ''
    return ''.join(text(c) for c in x)
data=json.loads((TASK/'evidence/qcc_design.json').read_text(encoding='utf-8'))
units=[{k:u[k] for k in ['id','title','module_id','input','output','process','missing_or_conflicting','requirement_candidates']} for u in data['units']]
(ROOT/'unit-source-contract.json').write_text(json.dumps(units,ensure_ascii=False,indent=2),encoding='utf-8')
for code in ['SAD','SDDD','SIP']:
    p=next((ROOT/'before').glob('[[]E8300-QCC[]] '+code+'*.docx'))
    with zipfile.ZipFile(p) as z:
        r=E.fromstring(z.read('word/document.xml'));b=r.find('w:body',NS)
        s=E.fromstring(z.read('word/styles.xml'))
        sm={n.find('w:name',NS).get(q('val')):n.get(q('styleId')) for n in s if n.find('w:name',NS)is not None}
        blocks=[{'block':i,'tag':e.tag.rsplit('}',1)[1],'text':text(e),'pPr':E.tostring(e.find('w:pPr',NS),encoding='unicode') if e.find('w:pPr',NS)is not None else ''}for i,e in enumerate(b,1)]
        wanted={'SAD':[73,74,75,77,78,136,137,138,171,173],'SDDD':[152,155,2567,2568,2569],'SIP':[99,100,154,155]}.get(code)
        (ROOT/(code+'-probe.json')).write_text(json.dumps({'style_ids':sm,'blocks':[x for x in blocks if x['block'] in wanted]},ensure_ascii=False,indent=2),encoding='utf-8')
req=json.loads((ROOT.parent/'qcc_requirements/requirements-contract.json').read_text(encoding='utf-8'))
(ROOT/'requirements-summary.txt').write_text('\n'.join(str(x['rid'])+' | '+x['F'].strip().replace('\n',' / ') for x in req['requirements']),encoding='utf-8')
print(json.dumps({'units':len(units),'reqs':len(req['requirements'])},ensure_ascii=True))
