import copy,hashlib,json,pathlib,zipfile
from lxml import etree as E
ROOT=pathlib.Path(r'E:\workspace\projects\sound1-fw-e8300-backup\docs\tasks\인허가\20261012_2102_QCC-설계문서-반영');evfile=ROOT/'evidence/qcc_requirements_authored.json';ev=json.loads(evfile.read_text(encoding='utf8'));plan=json.loads((ROOT/'evidence/qcc_requirements_change_plan.json').read_text(encoding='utf8'));out=pathlib.Path(ev['outputs'][0]['file']);oldhash=ev['outputs'][0]['sha256']
assert hashlib.sha256(out.read_bytes()).hexdigest().upper()==oldhash
with zipfile.ZipFile(out) as z:parts={n:z.read(n) for n in z.namelist()}
X='{http://schemas.openxmlformats.org/spreadsheetml/2006/main}';s=E.fromstring(parts['xl/worksheets/sheet2.xml']);cells={c.get('r'):c for c in s.iter(X+'c')};styles=E.fromstring(parts['xl/styles.xml']);xfs=styles.find(X+'cellXfs');mapping={};changed=[]
for edit in plan['cell_edits']:
 if not edit['cell'].startswith('I'):continue
 c=cells[edit['cell']];oldstyle=int(c.get('s','0'))
 if oldstyle not in mapping:
  xf=copy.deepcopy(xfs[oldstyle]);al=xf.find(X+'alignment')
  if al is None:al=E.SubElement(xf,X+'alignment')
  assert al.get('wrapText') is None
  al.set('wrapText','1');xf.set('applyAlignment','1');mapping[oldstyle]=len(xfs);xfs.append(xf)
 c.set('s',str(mapping[oldstyle]));changed.append(edit['cell'])
xfs.set('count',str(len(xfs)))
parts['xl/styles.xml']=E.tostring(styles,encoding='UTF-8',xml_declaration=True,standalone=True);parts['xl/worksheets/sheet2.xml']=E.tostring(s,encoding='UTF-8',xml_declaration=True,standalone=True)
assert hashlib.sha256(out.read_bytes()).hexdigest().upper()==oldhash
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 for n,v in parts.items():z.writestr(n,v)
newhash=hashlib.sha256(out.read_bytes()).hexdigest().upper();ev['outputs'][0]['sha256']=newhash;ev['outputs'][0]['source_column_wrapping']={'cells':changed,'existing_styles_preserved':True,'new_style_mapping':mapping};ev['outputs'][0]['unrelated_package_parts']='preserved from copied source except sheet2.xml, workbook.xml print ranges, calcChain.xml row references, original note row references/anchor, and added I-column wrapping style';evfile.write_text(json.dumps(ev,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps({'sha256':newhash,'wrap_cells':len(changed)},ensure_ascii=False))
