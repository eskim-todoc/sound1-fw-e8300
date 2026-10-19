from pathlib import Path
from lxml import etree as E
import json,zipfile,collections
from integrate import N
H=Path(__file__).resolve().parent
out={}
for name in ['canonical-rich','color-only-rich','font-size-rich','bare-rich']:
 with zipfile.ZipFile(H/('SUVC-probe-'+name+'.xlsx')) as z:r=E.fromstring(z.read('xl/worksheets/sheet2.xml'))
 rp=r.findall('.//x:c/x:is/x:r/x:rPr',N)
 out[name]={'example':E.tostring(r.find('.//x:c[@r="I1760"]',N),encoding='unicode'),'properties':sorted({E.tostring(x,encoding='unicode') for p in rp for x in p})}
print(json.dumps(out,ensure_ascii=True))
