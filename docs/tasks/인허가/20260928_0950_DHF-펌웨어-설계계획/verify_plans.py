"""Read-only package and content validation of the six deliverables."""
import json, posixpath
from zipfile import ZipFile
from build_plans import TASK, SOURCE, DEST, NS, xml, text, sha

reports=json.loads((TASK/'build_report.json').read_text(encoding='utf-8'))
edits=json.loads((TASK/'edits.json').read_text(encoding='utf-8'))
for report in reports:
    source=SOURCE/report['source']; target=DEST/report['target']
    assert sha(source.read_bytes())==report['source_sha256']
    assert sha(target.read_bytes())==report['target_sha256']
    with ZipFile(source) as old, ZipFile(target) as z:
        assert z.testzip() is None
        for name in z.namelist():
            if name.endswith('.xml') or name.endswith('.rels'): xml(z.read(name))
            if name.endswith('.rels'):
                root=xml(z.read(name)); base=posixpath.dirname(posixpath.dirname(name))
                for rel in root:
                    if rel.get('TargetMode')=='External': continue
                    dest=posixpath.normpath(posixpath.join(base,rel.get('Target'))).lstrip('/')
                    assert dest in z.namelist(),(target.name,dest)
        x=xml(z.read('word/document.xml')); paras=x.xpath('//w:p',namespaces=NS)
        code=target.name.split('-')[0]
        for index,edit in edits[code].items(): assert text(paras[int(index)])==edit[1]
        if code=='SDP':
            numbers=[text(paras[i]) for i in range(541,620,3)]
            assert numbers==list(map(str,range(1,28))),numbers
        for name in ('word/styles.xml','word/numbering.xml','word/fontTable.xml','word/theme/theme1.xml'):
            if name in old.namelist(): assert old.read(name)==z.read(name)
        body='\n'.join(text(p) for p in paras[:110])
        assert '임정우' not in body and '이호승' not in body and '2020.' not in body
        assert '김은수' in body and '수석연구원' in body
        assert '최초 제정' in body and 'Sound1 전환안' not in body
        assert '기존 제품 기준, 적용 확인' not in '\n'.join(text(p) for p in paras)
        for name in z.namelist():
            if name.startswith('word/header') and name.endswith('.xml'):
                assert '검토본' not in z.read(name).decode('utf-8')
        print(code, 'PASS: source hash, ZIP/XML/relationships, edited text, styles, approval reset')
print('All six packages passed. Visual evidence: tmp/dhf-plans-20260928/minimal-v3')
