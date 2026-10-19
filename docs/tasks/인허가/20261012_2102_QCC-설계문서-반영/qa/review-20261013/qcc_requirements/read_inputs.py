import json, pathlib, zipfile, hashlib, shutil, re
from lxml import etree as E
ROOT=pathlib.Path(__file__).parent
G=pathlib.Path(r'G:\공유 드라이브\20_프로젝트별\01_project sound1\08_DHF\02_설계 입력\SW\외부기')
X='http://schemas.openxmlformats.org/spreadsheetml/2006/main'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
NS={'x':X,'w':W}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def xread(p):
 with zipfile.ZipFile(p) as z:
  ss=[]
  if 'xl/sharedStrings.xml' in z.namelist():ss=[''.join(n.xpath('.//x:t/text()',namespaces=NS)) for n in E.fromstring(z.read('xl/sharedStrings.xml'))]
  wb=E.fromstring(z.read('xl/workbook.xml'));rels=E.fromstring(z.read('xl/_rels/workbook.xml.rels'))
  targets={n.get('Id'):n.get('Target') for n in rels};sheets=[]
  for s in wb.find('{'+X+'}sheets'):
   rel=s.get('{http://schemas.openxmlformats.org/officeDocument/2006/relationships}id');t=targets[rel];t=t.lstrip('/') if t.startswith('/') else 'xl/'+t
   doc=E.fromstring(z.read(t));rows=[]
   for row in doc.findall('.//{'+X+'}sheetData/{'+X+'}row'):
    cells={}
    for c in row:
     v=c.find('{'+X+'}v');f=c.find('{'+X+'}f');typ=c.get('t')
     val=''.join(c.xpath('.//x:t/text()',namespaces=NS)) if typ=='inlineStr' else ss[int(v.text)] if typ=='s' and v is not None else v.text if v is not None else ''
     cells[c.get('r')]={'value':val,'formula':f.text if f is not None else None,'style':c.get('s')}
    rows.append({'row':int(row.get('r')),'height':row.get('ht'),'cells':cells})
   sheets.append({'name':s.get('name'),'sheet_id':s.get('sheetId'),'part':t,'rows':rows,'merges':doc.xpath('//x:mergeCell/@ref',namespaces=NS)})
  return {'path':str(p),'sha256':sha(p),'sheets':sheets}
def wread(p):
 with zipfile.ZipFile(p) as z:
  d=E.fromstring(z.read('word/document.xml'));paras=[]
  for i,n in enumerate(d.xpath('//w:body//w:p',namespaces=NS)):
   current=n.xpath('.//w:t[not(ancestor::w:del or ancestor::w:moveFrom or ancestor::w:tr[w:trPr/w:del] or ancestor::w:tc[w:tcPr/w:cellDel])]/text()',namespaces=NS)
   toc=bool(n.xpath('ancestor::w:sdt[w:sdtPr/w:docPartObj/w:docPartGallery[@w:val="Table of Contents"]]',namespaces=NS))
   style=n.find('w:pPr/w:pStyle',NS)
   paras.append({'index':i,'text':''.join(current),'style':style.get('{'+W+'}val') if style is not None else None,'toc':toc,'active':not bool(n.xpath('ancestor::w:del|ancestor::w:moveFrom|ancestor::w:tr[w:trPr/w:del]|ancestor::w:tc[w:tcPr/w:cellDel]',namespaces=NS))})
  return {'path':str(p),'sha256':sha(p),'paragraphs':paras,'revision_counts':{t:len(d.xpath('//w:'+t,namespaces=NS)) for t in ['ins','del','pPrChange','rPrChange','tblPrChange']},'images':{n:hashlib.sha256(z.read(n)).hexdigest() for n in z.namelist() if n.startswith('word/media/')}}
matrix=pathlib.Path(r'C:\Users\EUNSU\AppData\Local\Temp\browser-use\exports\TD24A_설계_입출력_검증_매트릭스-6d980eb4-83bb-4643-98b1-197c64cbb726.xlsx')
shutil.copy2(matrix,ROOT/'matrix-readonly.xlsx')
result={'date':'2026-10-13','matrix':xread(matrix),'files':{}}
for pre in ['[E8300]','[E8300-QCC]']:
 for k,name in [('SRS','SRS-FW-TD24A 소프트웨어 요구사항 사양서.xlsx'),('SR','SR-FW-TD24A 소프트웨어 요구사항.docx'),('SGR','SGR-FW-TD24A 소프트웨어 일반요구사항.docx')]:
  p=G/(pre+' '+name);result['files'][pre+' '+k]=xread(p) if k=='SRS' else wread(p)
p=G/'서석일_작성'/'QCC_SRS-FW-TD24A 소프트웨어 요구사항 사양서_byssi.docx';result['files']['QCC SRS']=wread(p)
with zipfile.ZipFile(r'E:\workspace\rules-for-codex\지침\양식\Word\TDC 공통 스타일.dotx') as z:
 styles=E.fromstring(z.read('word/styles.xml'));result['dotx']={'styles':{n.get('{'+W+'}styleId'):E.tostring(n,encoding='unicode') for n in styles.xpath('//w:style[w:name[starts-with(@w:val,"TDC")]]',namespaces=NS)},'numbering':z.read('word/numbering.xml').decode('utf-8')}
(ROOT/'inspection.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'saved':str(ROOT/'inspection.json'),'matrix_sheets':[s['name'] for s in result['matrix']['sheets']],'matrix_sha':result['matrix']['sha256'],'files':[{'label':k,'sha256':v['sha256']} for k,v in result['files'].items()]},ensure_ascii=False))
