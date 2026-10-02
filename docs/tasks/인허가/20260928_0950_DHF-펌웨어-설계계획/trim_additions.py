"""Withdraw redundant proposed insertions, preserving original-based redlines."""
import json
from zipfile import ZipFile, ZIP_DEFLATED
from build_plans import DEST, TASK, NS, W, xml, serialize

edits=json.loads((TASK/'edits.json').read_text(encoding='utf-8'))
report=[]
pending=[]
for path in sorted(DEST.glob('*.docx')):
    code=path.name.split('-')[0]
    with ZipFile(path) as z:
        package={i.filename:z.read(i.filename) for i in z.infolist()}
    root=xml(package['word/document.xml'])
    changes=[]
    for key,entry in edits[code].items():
        label,before,reason,original=entry
        if reason!='원문 절차·필드를 유지하고 Sound1 적용 대상만 추가':
            continue
        after=original.replace('OTE','Sound1')
        if code=='SCMP' and key=='205':
            after=original+' 그림은 분기 구성의 예시이며, Sound1에서는 NRF 대신 QCC5181을 사용한다.'
        assert before.startswith(after),(code,key)
        matches=[p for p in root.xpath('//w:p',namespaces=NS)
                 if ''.join(p.xpath('.//w:t[not(ancestor::w:del)]/text()',namespaces=NS))==before]
        assert len(matches)==1,(code,key,'missing/ambiguous paragraph')
        p=matches[0]
        offset=0
        for node in p.xpath('.//w:t[not(ancestor::w:del)]',namespaces=NS):
            value=node.text or ''
            if offset+len(value)>len(after):
                assert node.xpath('ancestor::w:ins',namespaces=NS),(code,key,'would modify original text')
                node.text=value[:max(0,len(after)-offset)]
            offset+=len(value)
        for insertion in p.xpath('.//w:ins',namespaces=NS):
            if not ''.join(insertion.xpath('.//w:t/text()',namespaces=NS)):
                assert not insertion.xpath('.//w:drawing|.//w:pict',namespaces=NS)
                insertion.getparent().remove(insertion)
        assert ''.join(p.xpath('.//w:t/text()',namespaces=NS))==after
        entry[1]=after
        entry[2]='원문으로 충분하여 추가 설명 삭제' if after==original else '제품 식별 또는 기존 그림의 적용 차이만 반영'
        changes.append({'location':label,'removed':before[len(after):],'retained':after})
    assert changes
    assert root.xpath('//w:ins',namespaces=NS) and root.xpath('//w:del',namespaces=NS)
    assert '·' not in ''.join(root.xpath('//w:t/text()',namespaces=NS))
    package['word/document.xml']=serialize(root)
    for name,data in package.items():
        if name.startswith('word/') and name.endswith('.xml'):
            assert '·' not in ''.join(xml(data).xpath('//w:t/text()',namespaces=NS)),(path.name,name)
    pending.append((path,package))
    report.append({'file':path.name,'changes':changes,'remaining_insertions':len(root.xpath('//w:ins',namespaces=NS))})
assert len(pending)==6
for path,package in pending:
    temp=path.with_suffix('.docx.tmp')
    with ZipFile(temp,'w',ZIP_DEFLATED) as z:
        for name,data in package.items():z.writestr(name,data)
    temp.replace(path)
(TASK/'edits.json').write_text(json.dumps(edits,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(TASK/'trim_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
for item in report:print(item['file'],len(item['changes']),'paragraphs simplified')
