import pathlib,json,zipfile,hashlib
from lxml import etree as E
R=pathlib.Path(__file__).parent;D=json.loads((R/'inspection.json').read_text(encoding='utf-8'));p=pathlib.Path(D['files']['[E8300-QCC] SRS']['path'])
sha=lambda:hashlib.sha256(p.read_bytes()).hexdigest().upper()
before=sha();assert before=='E7DFB3B707E7F60241CB3D6752A9F93370A42CF4DF7C35014D991573E476A1B5'
with zipfile.ZipFile(p) as z:parts={n:z.read(n) for n in z.namelist()}
d=E.fromstring(parts['xl/worksheets/sheet2.xml']);ns={'x':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'};row=d.xpath('//x:row[@r="56"]',namespaces=ns)[0];old=row.get('ht');row.set('ht','200');row.set('customHeight','1')
parts['xl/worksheets/sheet2.xml']=E.tostring(d,encoding='UTF-8',xml_declaration=True,standalone=True)
assert sha()==before
with zipfile.ZipFile(p,'w',zipfile.ZIP_DEFLATED) as z:
 for n,b in parts.items():z.writestr(n,b)
proof={'before_sha256':before,'after_sha256':sha(),'row':56,'old_height':old,'new_height':200,'reason':'실제 Excel 전체 인쇄 10쪽에서 F56 첫 줄이 잘린 현상을 행 높이로 보완. 내용/추적/병합/서식은 유지.','native_recheck':'대기'}
(R/'srs-layout-fix.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(proof,ensure_ascii=False))
