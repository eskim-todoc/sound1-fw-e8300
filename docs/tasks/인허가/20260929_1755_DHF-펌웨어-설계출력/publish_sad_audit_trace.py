"""수정 범위와 지면 검토가 완료된 아키텍쳐 기술서를 같은 파일에 반영한다."""
import json
import re
import shutil
import zipfile
from pathlib import Path
from lxml import etree as E
from patch_sad_audit_trace import QA, parts
from inspect_sad_unit_trace import SAD, N, sha, text

sources=json.loads((QA/'sources.json').read_text(encoding='utf8'))
check=json.loads((QA/'verification.json').read_text(encoding='utf8'))
visual=json.loads((QA/'visual-review.json').read_text(encoding='utf8'))
assert check['requirements']==81 and check['unit_links']==272
assert check['qcc_prefix_count']==29 and check['new_unit_ids']==0
assert visual['reviewed_pages']==check['pages_to_review'] and visual['issues']==[]
assert all(sha(Path(p))==h for p,h in sources.items()), '검토 중 원본 변경 발생'
package=parts(QA/'candidate.docx')
root=E.fromstring(package['word/document.xml'])
accepted=E.fromstring(parts(QA/'candidate-accepted.docx')['word/document.xml'])
old=root.find('w:body/w:sdt',N).findall('.//w:p',N)
new=accepted.find('w:body/w:sdt',N).findall('.//w:p',N)
assert len(old)==len(new)
cache=[]
for a,b in zip(old,new):
    assert re.sub(r'[\d\s]','',text(a))==re.sub(r'[\d\s]','',text(b))
    ta=a.findall('.//w:t',N);tb=b.findall('.//w:t',N)
    if not ta: continue
    if ta[-1].text!=tb[-1].text:
        assert ta[-1].text.isdigit() and tb[-1].text.isdigit()
        cache.append([ta[-1].text,tb[-1].text]);ta[-1].text=tb[-1].text
package['word/document.xml']=E.tostring(root,encoding='UTF-8',xml_declaration=True,standalone=True)
staged=QA/'publish.docx'
with zipfile.ZipFile(staged,'w',zipfile.ZIP_DEFLATED) as z:
    for n,b in package.items():z.writestr(n,b)
temp=SAD.with_name(SAD.name+'.pending')
assert not temp.exists()
assert sha(SAD)==sources[str(SAD)]
shutil.copyfile(staged,temp)
temp.replace(SAD)
assert sha(SAD)==sha(staged)
assert all(sha(Path(p))==h for p,h in sources.items() if Path(p)!=SAD)
result={**check,'output':str(SAD),'sha256':sha(SAD),'toc_page_cache_changes':cache,
        'final_visual_check':True,'source_documents_and_firmware_unchanged':True}
(QA/'published-verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(result,ensure_ascii=False))
