import pathlib,json,zipfile,hashlib
from lxml import etree as E
R=pathlib.Path(__file__).parent;D=json.loads((R/'inspection.json').read_text(encoding='utf-8'));p=pathlib.Path(D['files']['[E8300-QCC] SRS']['path'])
sha=lambda:hashlib.sha256(p.read_bytes()).hexdigest().upper();before=sha();assert before=='468F5E305AD1386DD0A02868F3C1EB3C6D290F53406F4601BFB11C26CB337D30'
with zipfile.ZipFile(p) as z:parts={n:z.read(n) for n in z.namelist()}
d=E.fromstring(parts['xl/worksheets/sheet2.xml']);ns={'x':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
changes=[]
for n,height in {21:185,52:170,56:235,61:225,63:265,68:295,69:360,73:205}.items():
 row=d.xpath('//x:row[@r=$n]',namespaces=ns,n=str(n))[0];changes.append({'row':n,'old_height':float(row.get('ht')),'new_height':height});row.set('ht',str(height));row.set('customHeight','1')
parts['xl/worksheets/sheet2.xml']=E.tostring(d,encoding='UTF-8',xml_declaration=True,standalone=True);assert sha()==before
with zipfile.ZipFile(p,'w',zipfile.ZIP_DEFLATED) as z:
 for n,b in parts.items():z.writestr(n,b)
proof={'before_sha256':before,'after_sha256':sha(),'row_changes':changes,'content_merge_styles_unchanged':True,'reason':'전체 native27쪽을 직접 확인하여 RID15/44/47/48/52/54 분할 셀의 첫줄 잘림을 확인했다. RID69 재조정도 포함하며 정상 글꼴을 유지한 채 해당 행 높이만 보완했다.','native_recheck':'대기'}
(R/'srs-layout-fix2.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(proof,ensure_ascii=False))
