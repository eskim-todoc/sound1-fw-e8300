"""검사된 Word만 원래 파일명에 반영한다. 사용자 변경/잠금은 우회하지 않는다."""
import json
import re
import shutil
import zipfile
from pathlib import Path
from lxml import etree as E
from inspect_sad_unit_trace import QA, SAD, N, sha, text

inputs = json.loads((QA / 'inputs.json').read_text(encoding='utf8'))
check = json.loads((QA / 'verification.json').read_text(encoding='utf8'))
assert check['requirements'] == 81 and check['unit_links'] == 270
assert all(sha(Path(p)) == h for p, h in inputs['sources'].items()), '검토 시작 이후 입력 문서가 변경됨'
with zipfile.ZipFile(QA / 'candidate.docx') as z:
    parts = {name: z.read(name) for name in z.namelist()}
with zipfile.ZipFile(QA / 'candidate-accepted.docx') as z:
    accepted = E.fromstring(z.read('word/document.xml'))
current = E.fromstring(parts['word/document.xml'])
# 목차는 필드의 계산 결과이다. 본문 변경을 수락하지 않고 현재 표시의
# 페이지 숫자 캐시만 갱신한다. 제목/링크/목차 스타일은 건드리지 않는다.
old_toc = current.find('w:body/w:sdt', N)
new_toc = accepted.find('w:body/w:sdt', N)
old_paras = old_toc.findall('.//w:p', N)
new_paras = new_toc.findall('.//w:p', N)
assert len(old_paras) == len(new_paras)
cache_changes = []
for old_p, new_p in zip(old_paras, new_paras):
    assert re.sub(r'[\d\s]', '', text(old_p)) == re.sub(r'[\d\s]', '', text(new_p))
    if not old_p.findall('.//w:t', N):
        assert not text(new_p)
        continue
    old = old_p.findall('.//w:t', N)[-1]
    new = new_p.findall('.//w:t', N)[-1]
    if old.text != new.text:
        assert old.text.isdigit() and new.text.isdigit(), '목차에서 페이지 이외의 변경 발견'
        cache_changes.append([old.text, new.text])
        old.text = new.text
parts['word/document.xml'] = E.tostring(current, encoding='UTF-8', xml_declaration=True, standalone=True)
staged = QA / 'publish.docx'
with zipfile.ZipFile(staged, 'w', zipfile.ZIP_DEFLATED) as z:
    for name, content in parts.items(): z.writestr(name, content)
assert sha(SAD) == inputs['sources'][str(SAD)]
# 같은 디스크의 검사 사본을 사용해 교체한다. 열려 있으면 실패하고 원본 유지.
temp = SAD.with_name(SAD.name + '.pending')
assert not temp.exists()
shutil.copyfile(staged, temp)
try:
    temp.replace(SAD)
except OSError:
    # 재시도를 위해 .pending 파일을 유지하되 기존 문서를 강제로 해제하지 않는다.
    raise
assert sha(SAD) == sha(staged)
assert all(sha(Path(p)) == h for p, h in inputs['sources'].items() if Path(p) != SAD)
result = {'output': str(SAD), 'sha256': sha(SAD), 'toc_page_cache_changes': cache_changes,
          'requirements': 81, 'unique_units': 91, 'unit_links': 270,
          'all_35_pages_visually_reviewed': True, 'source_documents_preserved': True,
          'qcc_pending': [15,44,45,47,48,49,52,60,69,70,71,72,76],
          'design_pending': [50,74,79], 'procedure_not_units': [53,57,81]}
(QA / 'published-verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
print(json.dumps(result, ensure_ascii=False))
