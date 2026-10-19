import json,zipfile
from pathlib import Path
from lxml import etree as E
HERE=Path(__file__).resolve().parent;TASK=HERE.parent.parent
p=json.loads((HERE/'payload.json').read_text(encoding='utf-8'));b=next(b for b in p['books']if b['key']=='SSTR');out={}
n={'x':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
for key,fn in [('nativebase',HERE/'SSTR-native-baseline.xlsx'),('current',b['target'])]:
 with zipfile.ZipFile(fn)as z:
  out[key]={s:[E.QName(e).localname for e in E.fromstring(z.read(f'xl/worksheets/sheet{s}.xml'))]for s in range(1,6)}
  out[key]['styles']={E.QName(e).localname:[e.get('count'),len(e)]for e in E.fromstring(z.read('xl/styles.xml'))if e.get('count')}
  m=E.fromstring(z.read('xl/worksheets/sheet3.xml')).find('x:mergeCells',n)
  out[key]['history_merge']=E.tostring(m).decode()if m is not None else None
print(json.dumps(out,ensure_ascii=True))
(TASK/'evidence/qcc_tests_format_diagnosis.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
