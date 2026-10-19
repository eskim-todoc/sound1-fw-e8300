from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
from lxml import etree as E
from datetime import datetime,timezone
from copy import deepcopy
import json,hashlib
from style_patch import NS,q,_visible,_set
base=Path(__file__).parent
plan=json.loads((base/'authored.json').read_text('utf-8-sig'))
(base/'authored-before-refinement.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8')
proof=[]
for item in plan['docs']:
 code=item['code'];src=base/(code+'-candidate.docx')
 with ZipFile(src) as z:parts={n:z.read(n) for n in z.namelist()}
 d=E.fromstring(parts['word/document.xml']);ps=d.xpath('//w:body//w:p',namespaces=NS)
 oldtext=''.join(d.xpath('//w:t/text()|//w:delText/text()',namespaces=NS))
 oldprops=[E.tostring(x,method='c14n') for x in d.xpath('//w:pPrChange|//w:rPrChange',namespaces=NS)]
 rid=max(int(x) for x in d.xpath('//@w:id',namespaces=NS) if x.isdigit())+1
 date=datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00','Z')
 revisions=[]
 def attrs():
  global rid
  result={q('id'):str(rid),q('author'):'김은수',q('date'):date};revisions.append(str(rid));rid+=1;return result
 def delete_break_and_mark(i,mark=True):
  p=ps[i];pp=p.find('w:pPr',NS)
  for r in p.xpath('./w:r[w:br[@w:type="page"]]',namespaces=NS):
   assert not r.xpath('./w:t',namespaces=NS)
   at=p.index(r);p.remove(r);dele=E.Element(q('del'),attrs());dele.append(r);p.insert(at,dele)
  if mark and pp.find('w:rPr/w:del',NS) is None:
   rp=pp.find('w:rPr',NS)
   if rp is None:rp=E.SubElement(pp,q('rPr'))
   E.SubElement(rp,q('del'),attrs())
 def bold_keep(i):
  p=ps[i];_set(p.find('w:pPr',NS),'keepNext',{})
  for r in p.xpath('.//w:r[w:t and not(ancestor::w:del)]',namespaces=NS):
   rp=r.find('w:rPr',NS)
   if rp is None:rp=E.Element(q('rPr'));r.insert(0,rp)
   _set(rp,'b',{})
 changes=[]
 if code=='SR':
  # Continuous document pagination agrees with NUMPAGES in the preserved header.
  for sect in d.xpath('//w:sectPr',namespaces=NS)[1:]:
   pn=sect.find('w:pgNumType',NS)
   if pn is not None:pn.attrib.pop(q('start'),None)
  delete_break_and_mark(186)
  for i in [197,211,217,220,223,236,238,240,243,244,246]:bold_keep(i)
  for i in [218,219,221,222,224,225,226]:
   pp=ps[i].find('w:pPr',NS);_set(pp,'pStyle',{'val':'TDC20'});_set(pp,'ind',{'left':198,'right':0,'hanging':0})
  for i in [219,222]:
   # These two live layout tabs are within old insertions. Remove only the tab;
   # the old insertion's text, identity, author, date and format snapshots survive.
   tabs=ps[i].xpath('.//w:tab[not(ancestor::w:del)]',namespaces=NS);assert len(tabs)==1
   tabs[0].getparent().remove(tabs[0])
  for i in [277,281,285,289,293,297]:
   bold_keep(i);_set(ps[i+1].find('w:pPr',NS),'keepNext',{})
  changes=['Continuous page numbering','Removed obsolete standalone page break before hardware chapter','Bold existing bullet/category/question labels','Consistent 0.35cm memory item indentation; removed two layout tabs']
 else:
  # Old manual break plus empty paragraph generated a blank page after the
  # full history table. Place the required break on the actual introduction.
  delete_break_and_mark(134);delete_break_and_mark(135)
  _set(ps[136].find('w:pPr',NS),'pageBreakBefore',{})
  changes=['Moved introduction page break from two blank paragraphs to actual heading']
 assert ''.join(d.xpath('//w:t/text()|//w:delText/text()',namespaces=NS))==oldtext
 assert [E.tostring(x,method='c14n') for x in d.xpath('//w:pPrChange|//w:rPrChange',namespaces=NS)]==oldprops
 parts['word/document.xml']=E.tostring(d,encoding='UTF-8',xml_declaration=True,standalone=True)
 target=base/(code+'-refined.docx')
 with ZipFile(target,'w',ZIP_DEFLATED) as z:
  for n,data in parts.items():z.writestr(n,data)
 item['candidate']=str(target);item['candidate_sha256']=hashlib.sha256(target.read_bytes()).hexdigest().upper()
 item['layout_refinement']={'changes':changes,'new_structural_revision_ids':revisions,'word_and_deleted_text_unchanged':True,'prior_format_snapshots_preserved':True}
 proof.append({'code':code,'path':str(target),'sha256':item['candidate_sha256'],**item['layout_refinement']})
(base/'layout-refinement.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8')
(base/'authored.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(proof,ensure_ascii=False,indent=2))
