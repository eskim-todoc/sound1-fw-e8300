import hashlib,json,pathlib,zipfile
from lxml import etree as E
ROOT=pathlib.Path(r'E:\workspace\projects\sound1-fw-e8300-backup\docs\tasks\인허가\20261012_2102_QCC-설계문서-반영')
evfile=ROOT/'evidence/qcc_requirements_authored.json';ev=json.loads(evfile.read_text(encoding='utf8'));out=pathlib.Path(ev['outputs'][0]['file']);oldhash=ev['outputs'][0]['sha256']
assert hashlib.sha256(out.read_bytes()).hexdigest().upper()==oldhash
with zipfile.ZipFile(out) as z:parts={n:z.read(n) for n in z.namelist()}
X='{http://schemas.openxmlformats.org/spreadsheetml/2006/main}';V='{urn:schemas-microsoft-com:office:excel}'
comments=E.fromstring(parts['xl/comments1.xml']);c=comments.find('.//'+X+'comment');assert c.get('ref')=='B54';c.set('ref','B55')
parts['xl/comments1.xml']=E.tostring(comments,encoding='UTF-8',xml_declaration=True,standalone=True)
vml=E.fromstring(parts['xl/drawings/vmlDrawing1.vml']);row=vml.find('.//'+V+'Row');assert row.text=='53';row.text='54'
anchor=vml.find('.//'+V+'Anchor');nums=[int(x.strip()) for x in anchor.text.split(',')];assert nums[2]==52 and nums[6]==58;nums[2]+=1;nums[6]+=1;anchor.text=', '.join(map(str,nums))
parts['xl/drawings/vmlDrawing1.vml']=E.tostring(vml,encoding='UTF-8',xml_declaration=True,standalone=True)
assert hashlib.sha256(out.read_bytes()).hexdigest().upper()==oldhash
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 for n,v in parts.items():z.writestr(n,v)
newhash=hashlib.sha256(out.read_bytes()).hexdigest().upper();ev['outputs'][0]['sha256']=newhash;ev['outputs'][0]['original_note_shift']={'old_cell':'B54','new_cell':'B55','comment_author_and_text_preserved':True,'vml_anchor_row_shifted':True};ev['outputs'][0]['unrelated_package_parts']='preserved from copied source except sheet2.xml, workbook.xml print ranges, calcChain.xml row references, and original note row references/anchor'
evfile.write_text(json.dumps(ev,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps({'sha256':newhash,'comment_shift':'B54 -> B55'},ensure_ascii=False))
