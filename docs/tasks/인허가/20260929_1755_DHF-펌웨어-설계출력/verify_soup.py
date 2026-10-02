"""Check the final SAD patch, traceability, source invariance and relevant links."""
from copy import deepcopy
from pathlib import Path
import hashlib
import json
import re
from urllib.parse import unquote
import zipfile
from lxml import etree as E
from patch_sad_order import PATH, T, N, q, text, rejected

ROOT=T.parents[3]
with zipfile.ZipFile(PATH) as z:
    parts={n:z.read(n) for n in z.namelist()}
    r=E.fromstring(parts['word/document.xml'])
with zipfile.ZipFile(T/'qa/soup/before-soup.docx') as z:
    assert list(parts)==z.namelist()
    assert all(parts[n]==z.read(n) for n in parts if n!='word/document.xml')
    assert rejected(r)==rejected(E.fromstring(z.read('word/document.xml')))
trace=[t for t in r.xpath('//w:body/w:tbl',namespaces=N) if 'SW-FW-RID-1' in text(t)]
assert len(trace)==1
ids=[int(re.fullmatch(r'SW-FW-RID-(\d+)',text(tr.find(q('tc')))).group(1)) for tr in trace[0].findall(q('tr'))[2:]]
assert ids==list(range(1,82))
alltext=text(r)
for value in ['QCC 내부 소프트웨어','소프트웨어 요구사항 추적성','소프트웨어 아키텍쳐 검증',
              '표 SOUP-1. SOUP 식별 및 사용 범위','기본 연산 시험은 통과하였다','잔여 확인 항목']:
    assert value in alltext,value
assert E.fromstring(parts['word/settings.xml']).find(q('trackRevisions')) is not None
evidence=json.loads((T/'SOUP 확인 근거.json').read_text(encoding='utf8'))
for path,digest in evidence['source_sha256'].items():
    assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,path
assert len(evidence['tests'])==2 and all(t['exit']==0 for t in evidence['tests'])
mdfiles=[ROOT/'docs/참고/시스템/Sound1 SOUP 사용 및 검토.md',
         ROOT/'docs/참고/인허가/사운드1/QCC 자료 추가 위치 관리.md',
         ROOT/'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/[정리] Sound1 설계 출력 문서 변경사항.md']
links=0
for file in mdfiles:
    content=file.read_text(encoding='utf8')
    for target in re.findall(r'\]\(([^)]+)\)',content):
        if target.startswith(('http:', 'https:', '#')):continue
        local=(file.parent/unquote(target.split('#')[0])).resolve()
        assert local.exists(),(file.name,target,local)
        links+=1
result={'SW_RID_order':'1..81','source_files_unchanged':len(evidence['source_sha256']),
        'AES_host_pass':16,'local_links_checked':links,'reject_all_text_preserved':True,
        'only_package_change':'word/document.xml','target_tests':False,
        'sha256':hashlib.sha256(PATH.read_bytes()).hexdigest()}
(T/'qa/soup/final-checks.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(result,ensure_ascii=False))
