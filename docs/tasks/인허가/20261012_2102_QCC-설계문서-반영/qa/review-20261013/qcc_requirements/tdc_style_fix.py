import pathlib,zipfile,copy,json,hashlib,sys
from lxml import etree as E
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';q=lambda n:'{'+W+'}'+n;NS={'w':W}
ROOT=pathlib.Path(__file__).parent;DOTX=pathlib.Path('E:/workspace/rules-for-codex/지침/양식/Word/TDC 공통 스타일.dotx')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def repair(target,proofpath):
 target=pathlib.Path(target);start=sha(target)
 with zipfile.ZipFile(target) as z:parts={n:z.read(n) for n in z.namelist()}
 with zipfile.ZipFile(DOTX) as z:source=E.fromstring(z.read('word/styles.xml'))
 dest=E.fromstring(parts['word/styles.xml']);oldstyle=copy.deepcopy(dest);templates={s.find(q('name')).get(q('val')):s for s in source if s.tag==q('style') and s.find(q('name')) is not None};saved={s.find(q('name')).get(q('val')):s for s in dest if s.tag==q('style') and s.find(q('name')) is not None};canonical={n:s for n,s in templates.items() if n.startswith('TDC')};assert len(canonical)==59
 mapping={s.get(q('styleId')):saved[n].get(q('styleId')) for n,s in canonical.items()}
 support={'a1':'TDCSupport20261013Paragraph','a3':'TDCSupport20261013Table'};mapping.update(support)
 for sid,newid in support.items():
  original=source.xpath('./w:style[@w:styleId=$sid]',namespaces=NS,sid=sid)[0];style=copy.deepcopy(original);style.set(q('styleId'),newid);style.attrib.pop(q('default'),None);style.find(q('name')).set(q('val'),'공통 양식 본문 기반' if sid=='a1' else '공통 양식 표 기반')
  old=dest.xpath('./w:style[@w:styleId=$sid]',namespaces=NS,sid=newid)
  if old:dest.remove(old[0])
  dest.append(style)
 for name,template in canonical.items():
  old=saved[name];new=copy.deepcopy(template);new.set(q('styleId'),old.get(q('styleId')))
  for e in new.xpath('./w:basedOn|./w:link|./w:next',namespaces=NS):
   ref=e.get(q('val'));e.set(q('val'),mapping.get(ref,ref))
  # Keep the document's existing heading numbering linkage; never import template numId 0/11.
  if name.startswith('TDC 제목 '):
   pp=new.find(q('pPr'));oldnum=old.find('w:pPr/w:numPr',NS)
   for e in list(pp):
    if e.tag==q('numPr'):pp.remove(e)
   if oldnum is not None:pp.insert(0,copy.deepcopy(oldnum))
  at=dest.index(old);dest.remove(old);dest.insert(at,new)
 # Assert unrelated style definitions survive byte-equivalent canonical XML.
 imported={s.get(q('styleId')) for s in saved.values() if s.find(q('name')).get(q('val')).startswith('TDC')}
 for style in oldstyle.findall(q('style')):
  sid=style.get(q('styleId'))
  if sid in imported or sid in support.values():continue
  found=dest.xpath('./w:style[@w:styleId=$sid]',namespaces=NS,sid=sid)[0];assert E.tostring(style,method='c14n')==E.tostring(found,method='c14n')
 parts['word/styles.xml']=E.tostring(dest,encoding='UTF-8',xml_declaration=True,standalone=True)
 before_xml=hashlib.sha256(parts['word/document.xml']).hexdigest();assert sha(target)==start
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in parts.items():z.writestr(n,b)
 with zipfile.ZipFile(target) as z:assert hashlib.sha256(z.read('word/document.xml')).hexdigest()==before_xml
 proof={'target':str(target),'before_sha256':start,'after_sha256':sha(target),'canonical_tdc_count':59,'support_style_ids':support,'source_dotx_sha256':sha(DOTX),'unrelated_styles_unchanged':True,'document_text_revision_fields_images_xml_unchanged':True,'tdc_current_style_ids_preserved':True,'existing_heading_numbering_linkage_preserved':True,'base_normal_line':'300 auto = 1.25배','base_normal_after':'160 twips = 8pt','native_recheck':'대기'}
 pathlib.Path(proofpath).write_text(json.dumps(proof,ensure_ascii=False,indent=2),encoding='utf-8');return proof
if __name__=='__main__':
 D=json.loads((ROOT/'inspection.json').read_text(encoding='utf-8'))
 for name in ['SR','SGR']:print(json.dumps(repair(D['files']['[E8300-QCC] '+name]['path'],ROOT/(name+'-tdc-style-fix.json')),ensure_ascii=False))
