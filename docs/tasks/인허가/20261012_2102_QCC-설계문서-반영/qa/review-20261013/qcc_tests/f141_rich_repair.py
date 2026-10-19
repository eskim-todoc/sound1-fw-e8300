from pathlib import Path
from lxml import etree as E
import json,zipfile,copy
from integrate import sheetmap,N,vals,sha
H=Path(__file__).resolve().parent;P=json.loads((H/'author-payload.json').read_text(encoding='utf-8'));loaded={}
for k in ['SITC','SITR']:
 b=P['books'][k]
 with zipfile.ZipFile(b['path']) as z:infos=z.infolist();d={i.filename:z.read(i.filename) for i in infos}
 sn=next(s for s in sheetmap(d) if s.startswith(('기준서_','보고서_')));member=sheetmap(d)[sn];root=E.fromstring(d[member]);loaded[k]=(b,infos,d,sn,member,root)
b,infos,d,sn,member,root=loaded['SITR'];before=d.copy();beforevals=vals(d,member);isn=root.find('.//x:c[@r="F141"]/x:is',N);criterion=loaded['SITC'][-1].find('.//x:c[@r="F141"]/x:is',N);isn.getparent().replace(isn,copy.deepcopy(criterion));d[member]=E.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
assert vals(d,member)==beforevals and all(d[m]==before[m] for m in d if m!=member)
assert E.tostring(root.find('.//x:c[@r="F141"]/x:is',N))==E.tostring(criterion)
candidate=H/'SITR-f141-rich-repair.xlsx'
with zipfile.ZipFile(candidate,'w') as z:
 for i in infos:z.writestr(i,d[i.filename])
record={'path':b['path'],'sha256_before':sha(b['path']),'candidate':str(candidate),'sha256_candidate':sha(candidate),'changed_cell':'F141','cause':'SITC original font includes scheme minor; SITR original font omits scheme. Newly authored common rich text used these different font metadata.','correction':'Copy the criterion F141 inline rich content exactly; same text, font name/size/color and line breaks; font scheme metadata now identical.','values_formulas_merges_rows_heights_results_and_other_package_members_unchanged':True,'native_display':'pending root after COM closes'}
(H/'f141-rich-repair.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(record,ensure_ascii=True))
