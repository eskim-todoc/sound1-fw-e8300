import pathlib,zipfile,copy,uuid,json
from lxml import etree as E
R=pathlib.Path(__file__).parent;p=R/'SR-candidate.docx';W='http://schemas.openxmlformats.org/wordprocessingml/2006/main';q=lambda n:'{'+W+'}'+n
ns={'w':W,'wp':'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing','pic':'http://schemas.openxmlformats.org/drawingml/2006/picture','a16':'http://schemas.microsoft.com/office/drawing/2014/main','wp14':'http://schemas.microsoft.com/office/word/2010/wordprocessingDrawing'}
with zipfile.ZipFile(p) as z:parts={n:z.read(n) for n in z.namelist()}
for variant in ['unique-picture-ids','inner-picture-tracks','no-new-picture']:
 pp=copy.deepcopy(parts);d=E.fromstring(pp['word/document.xml']);p=d.xpath('//w:body//w:p',namespaces=ns)[196];deleted=p.find(q('del'));new=p.find(q('ins'))
 if variant=='unique-picture-ids':
  for e in new.xpath('.//wp:docPr|.//pic:cNvPr',namespaces=ns):e.set('id','1101');e.set('descr','DMIC2가 E8300과 QCC에 함께 입력되고 통화 상향과 I2S 하향이 구분되는 외부기 구조')
  for e in new.xpath('.//a16:creationId',namespaces=ns):e.set('id','{'+str(uuid.uuid4()).upper()+'}')
  for e in new.xpath('.//wp:inline',namespaces=ns):e.set('{'+ns['wp14']+'}anchorId','A1F43001');e.set('{'+ns['wp14']+'}editId','A1F43002')
 elif variant=='inner-picture-tracks':
  oldins=deleted.find(q('ins'));assert oldins is not None
  oldruns=list(oldins)
  for e in oldruns:oldins.remove(e)
  deleted.remove(oldins);p.remove(deleted);p.remove(new)
  for e in oldruns:deleted.append(e)
  oldins.extend([deleted,new]);p.append(oldins)
 elif variant=='no-new-picture':
  oldins=deleted.find(q('ins'));deleted.remove(oldins);p.remove(deleted);p.remove(new);p.append(oldins)
 pp['word/document.xml']=E.tostring(d,encoding='UTF-8',xml_declaration=True,standalone=True)
 target=R/('SR-'+variant+'.docx')
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in pp.items():z.writestr(n,b)
 print(str(target))
