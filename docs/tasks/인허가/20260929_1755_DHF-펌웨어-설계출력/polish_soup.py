"""Apply local layout fixes to the newly inserted SOUP content only."""
import zipfile
from lxml import etree as E
from patch_sad_order import PATH, N, q, text

with zipfile.ZipFile(PATH) as z:parts={n:z.read(n) for n in z.namelist()}
r=E.fromstring(parts['word/document.xml'])
body=r.find(q('body'));active=False
for node in body:
    if text(node).startswith('표 SOUP-1.'):
        active=True
    if not active:continue
    ps=[node] if node.tag==q('p') else node.xpath('.//w:p',namespaces=N)
    for p in ps:
        pp=p.find(q('pPr'))
        if pp is None:pp=E.SubElement(p,q('pPr'))
        widow=pp.find(q('widowControl'))
        if widow is None:widow=E.SubElement(pp,q('widowControl'))
        widow.set(q('val'),'1')
        jc=pp.find(q('jc'))
        if jc is None:E.SubElement(pp,q('jc'),{q('val'):'left'})
        for t in list(p.xpath('.//w:t',namespaces=N)):
            if '\n' not in (t.text or ''):continue
            strings=t.text.split('\n');t.text=strings[0]
            parent=t.getparent();pos=parent.index(t)+1
            for value in strings[1:]:
                parent.insert(pos,E.Element(q('br')));pos+=1
                new=E.Element(q('t'));new.text=value;parent.insert(pos,new);pos+=1
parts['word/document.xml']=E.tostring(r,encoding='UTF-8',xml_declaration=True,standalone=True)
with zipfile.ZipFile(PATH,'w',zipfile.ZIP_DEFLATED) as z:
    for name,value in parts.items():z.writestr(name,value)
print('SOUP-only paragraph widow protection, left alignment and source line breaks applied')
