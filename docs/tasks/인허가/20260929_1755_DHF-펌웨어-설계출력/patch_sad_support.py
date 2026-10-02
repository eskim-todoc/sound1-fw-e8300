"""Condense pending SOUP additions into an automatically numbered chapter."""
from copy import deepcopy
import zipfile
from lxml import etree as E
from patch_sad_order import PATH,T,q,N,text,rejected

qa=T/'qa/support-software'
qa.mkdir(parents=True,exist_ok=True)
backup=qa/'before.docx'
assert not backup.exists()
raw=PATH.read_bytes();backup.write_bytes(raw)
with zipfile.ZipFile(PATH) as z: parts={n:z.read(n) for n in z.namelist()}
root=E.fromstring(parts['word/document.xml']);before=deepcopy(root)
body=root.find(q('body'))
heading=next(p for p in body.findall(q('p')) if text(p)=='소프트웨어 아키텍쳐 검증')
answer=next(p for p in body.findall(q('p')) if text(p).startswith(': FatFs, tiny-AES-c'))
caption=next(p for p in body.findall(q('p')) if text(p)=='표 SOUP-1. SOUP 식별 및 사용 범위')
table=body[body.index(caption)+1]
assert table.tag==q('tbl')
new_heading=deepcopy(heading)
for c in list(new_heading):
    if c.tag!=q('pPr'):new_heading.remove(c)
pp=new_heading.find(q('pPr'))
rp=E.SubElement(pp,q('rPr'))
counter=max(int(x) for x in root.xpath('//@w:id',namespaces=N) if x.isdigit())+1
proto=deepcopy(caption.find(q('ins')))
def insertion():
    global counter
    result=deepcopy(proto);result.set(q('id'),str(counter));counter+=1
    return result
mark=insertion()
for c in list(mark):mark.remove(c)
rp.append(mark)
ins=insertion()
ins.find('.//'+q('t')).text='지원 소프트웨어'
new_heading.append(ins)
new_caption=deepcopy(caption)
new_caption.find('.//'+q('t')).text='표 SOUP-1. 지원 소프트웨어 목록'
new_table=deepcopy(table)
rows=[
 ['소프트웨어','버전','출처'],
 ['FatFs / 파일 시스템','R0.15 patch1','ChaN (onsemi SDK 배포)\nhttps://elm-chan.org/fsw/ff/'],
 ['tiny-AES-c / AES 암복호화','1.0.0 (패키지 표기)','kokke\nhttps://github.com/kokke/tiny-AES-c'],
 ['E8300 SDK CRC 지원 코드','Ezairo 8300 SDK\n4.5.0.107','onsemi / Ezairo 8300 SDK'],
 ['SEGGER RTT / 디버그 입출력','7.56a','SEGGER\nhttps://kb.segger.com/RTT']]
widths=[2500,2200,4170]
for col,width in zip(new_table.find(q('tblGrid')),widths):col.set(q('w'),str(width))
for row,values in zip(new_table.findall(q('tr')),rows):
    for cell,value,width in zip(row.findall(q('tc')),values,widths):
        cell.find(q('tcPr')).find(q('tcW')).set(q('w'),str(width))
        p=cell.find(q('p'));ins=p.find(q('ins'));run=ins.find(q('r'))
        for t in list(run):
            if t.tag!=q('rPr'):run.remove(t)
        for i,line in enumerate(value.split('\n')):
            if i:E.SubElement(run,q('br'))
            E.SubElement(run,q('t')).text=line
# Remove only the preceding turn's pending additions after the answer.
for element in list(body)[body.index(caption):]:
    if element.tag==q('sectPr'):continue
    assert element is table or element.xpath('.//w:ins',namespaces=N)
    body.remove(element)
answer.find('.//w:ins//w:t',namespaces=N).text=': 예, 사용되는 지원 소프트웨어의 종류, 버전 및 출처를 식별하여 관리한다.'
pos=body.index(heading)
for element in [new_heading,new_caption,new_table]:
    body.insert(pos,element);pos+=1
# Allocate distinct revision IDs for copied insertion nodes.
for element in [new_heading,new_caption,new_table]:
    for ins in element.xpath('.//w:ins',namespaces=N):
        ins.set(q('id'),str(counter));counter+=1
assert rejected(root)==rejected(before)
old_tables=before.xpath('//w:body/w:tbl',namespaces=N)[:-1]
assert all(E.tostring(a)==E.tostring(b) for a,b in zip(old_tables,root.xpath('//w:body/w:tbl',namespaces=N)))
parts['word/document.xml']=E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
candidate=qa/'candidate.docx'
with zipfile.ZipFile(candidate,'w',zipfile.ZIP_DEFLATED) as z:
    for n,data in parts.items():z.writestr(n,data)
assert PATH.read_bytes()==raw
PATH.write_bytes(candidate.read_bytes())
print('Support chapter inserted; automatic numbering retained; original source rejection and existing tables preserved.')
