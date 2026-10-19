from build_inputs import *
for code in ['SR','SGR']:
 parts=readzip(Q/(code+'-before.docx'));doc=E.fromstring(parts['word/document.xml'])
 for i,s in enumerate(doc.xpath('//w:body/w:sdt',namespaces=N)):
  print(code,'sdt',i,'paras',len(s.xpath('.//w:p',namespaces=N)),'revision_types',[E.QName(x).localname for x in s.xpath('.//*[@w:author]',namespaces=N)],'fields',s.xpath('.//w:instrText/text()',namespaces=N)[:4])
