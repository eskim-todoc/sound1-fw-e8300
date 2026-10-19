from probe_layout import base,ZipFile,E,NS,q
for code,ids in [('SR',[218,219,221,222]),('SGR',[134,135])]:
 with ZipFile(base/(code+'-candidate.docx')) as z:d=E.fromstring(z.read('word/document.xml'))
 ps=d.xpath('//w:body//w:p',namespaces=NS)
 for i in ids:
  for r in ps[i].xpath('.//w:r[w:tab or w:br]',namespaces=NS):
   print(code,i,[(E.QName(a).localname,a.get(q('id'))) for a in r.iterancestors()][:3],E.tostring(r,encoding='unicode')[-1600:])
