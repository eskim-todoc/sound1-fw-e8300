"""Sort the pending SAD traceability table and qualify confirmed review statements."""
from pathlib import Path
from copy import deepcopy
from datetime import datetime, timezone
import hashlib
import json
import re
import zipfile
from lxml import etree as E

T=Path(__file__).resolve().parent
ROOT=T.parents[3]
PATH=ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서.docx'
QA=T/'qa/sad-order'
W='http://schemas.openxmlformats.org/wordprocessingml/2006/main'
N={'w':W}
def q(name): return '{'+W+'}'+name
def text(x):return ''.join(x.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=N))
def rejected(root):
    root=deepcopy(root)
    for row in root.xpath('//w:tr[w:trPr/w:ins]',namespaces=N):row.getparent().remove(row)
    for node in root.xpath('//w:ins',namespaces=N):node.getparent().remove(node)
    for node in root.xpath('//w:del',namespaces=N):
        parent=node.getparent();i=parent.index(node)
        for child in list(node):parent.insert(i,child);i+=1
        parent.remove(node)
    return ''.join(root.xpath('.//w:t/text()|.//w:delText/text()',namespaces=N))

def main():
    QA.mkdir(parents=True,exist_ok=True)
    backup=QA/'before.docx'
    assert not backup.exists(),'Existing baseline; inspect before rerunning.'
    raw=PATH.read_bytes();backup.write_bytes(raw)
    with zipfile.ZipFile(PATH) as z:parts={n:z.read(n) for n in z.namelist()}
    root=E.fromstring(parts['word/document.xml']);before=deepcopy(root)
    tables=[x for x in root.xpath('//w:body/w:tbl',namespaces=N) if 'SW-FW-RID-1' in text(x)]
    assert len(tables)==1
    table=tables[0];rows=table.findall(q('tr'));data=rows[2:]
    def id_of(row):return int(re.fullmatch(r'SW-FW-RID-(\d+)',text(row.find(q('tc')))).group(1))
    assert sorted(map(id_of,data))==list(range(1,82))
    orig_rows={id_of(row):E.tostring(row) for row in data}
    for row in data:table.remove(row)
    for row in sorted(data,key=id_of):table.append(row)
    assert all(E.tostring(row)==orig_rows[id_of(row)] for row in data)
    # Only newly inserted requirements 69..81 move past existing source rows.
    # Existing rows 1..68 retain their original relative order on Reject All.
    changes={
      ': 소프트웨어 요구사항 사양서의 81개 요구사항을 추적성 표에 연결한다. QCC 단독 요구사항은 QCC 내부 설계가 추가될 때 해당 항목과의 연결을 확인한다.':
      ': 예, 소프트웨어 요구사항 사양서의 81개 요구사항이 추적성 표에 모두 포함되어 있음을 확인하였다. QCC 단독 요구사항의 내부 설계 연결은 QCC 자료 추가 후 확인한다.',
      ': E8300의 공유 메모리, SPI, I2S 및 제어 핀을 기준으로 모듈과 하드웨어 사이의 연계를 확인한다.':
      ': 예, E8300의 공유 메모리, SPI, I2S 및 제어 핀을 기준으로 모듈과 하드웨어 사이의 연계를 확인하였다. QCC 내부 모듈 간 연계는 관련 자료 추가 후 확인한다.'}
    next_id=max(int(x) for x in root.xpath('//@w:id',namespaces=N) if x.isdigit())+1
    stamp=datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    for old,new in changes.items():
        hits=[p for p in root.xpath('//w:body/w:p',namespaces=N) if text(p)==old]
        assert len(hits)==1,(old,len(hits))
        p=hits[0];original_text=rejected(p)
        proto=p.find('.//'+q('rPr'))
        proto=deepcopy(proto) if proto is not None else None
        for c in list(p):
            if c.tag!=q('pPr'):p.remove(c)
        for kind,value in [('del',original_text),('ins',new)]:
            node=E.SubElement(p,q(kind),{q('id'):str(next_id),q('author'):'김은수',q('date'):stamp});next_id+=1
            run=E.SubElement(node,q('r'))
            if proto is not None:run.append(deepcopy(proto))
            txt=E.SubElement(run,q('delText' if kind=='del' else 't'));txt.text=value
            txt.set('{http://www.w3.org/XML/1998/namespace}space','preserve')
    assert rejected(root)==rejected(before),'Reject All source text/order changed'
    soup_before=[E.tostring(p) for p in before.xpath('//w:body/w:p',namespaces=N) if 'SOUP' in text(p) or '칩셋 제공 라이브러리' in text(p)]
    soup_after=[E.tostring(p) for p in root.xpath('//w:body/w:p',namespaces=N) if 'SOUP' in text(p) or '칩셋 제공 라이브러리' in text(p)]
    assert soup_before==soup_after
    parts['word/document.xml']=E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
    candidate=QA/'candidate.docx'
    with zipfile.ZipFile(candidate,'w',zipfile.ZIP_DEFLATED) as z:
        for name,value in parts.items():z.writestr(name,value)
    assert hashlib.sha256(PATH.read_bytes()).digest()==hashlib.sha256(raw).digest(),'Source changed during edit'
    PATH.write_bytes(candidate.read_bytes())
    with zipfile.ZipFile(backup) as a,zipfile.ZipFile(PATH) as b:
        assert a.namelist()==b.namelist()
        assert all(a.read(n)==b.read(n) for n in a.namelist() if n!='word/document.xml')
    result={'requirements':81,'ordered_ids':list(map(id_of,table.findall(q('tr'))[2:])),
            'row_content_and_format_unchanged':True,'review_paragraphs_updated':2,
            'SOUP_unchanged':True,'reject_all_source_text_preserved':True,
            'only_changed_package_part':'word/document.xml'}
    (QA/'checks.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(result,ensure_ascii=False))

if __name__=='__main__':main()
