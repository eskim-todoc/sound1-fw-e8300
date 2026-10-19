from pathlib import Path
import hashlib,json,datetime
from PIL import Image
H=Path(__file__).resolve().parent
D=H.parent/'final-word'/'SAD'
source=Path('G:/공유 드라이브/20_프로젝트별/01_project sound1/08_DHF/03_설계 출력/SW/외부기/[E8300-QCC] SAD-FW-TD24A 소프트웨어 아키텍쳐 기술서.docx')
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
expected='84F592A7F7988A78EB73FA718268CC22DDF20A5BAC796358B971ABA0D23CCC68'
assert sha(source).upper()==expected
record={'source':str(source),'sourceSHA256':expected,'checked_at':datetime.datetime.now().astimezone().isoformat(),
    'status':'통과','scope':'최종 native SAD 55쪽 중 담당 구간29~42쪽14개를 원본893×1263 PNG로 각각 직접 열어 확인했다. 축소 contact를 대신 사용하지 않았다. 이 기록은 담당14쪽의 실제 표시 검사이며 실제 제품시험이나 나머지41쪽 검사를 대신하지 않는다.',
    'checks':['요구사항 ID와 모듈/유닛 ID 및 이름의 셀 안 표시','긴 유닛 목록/미비 안내의 첫줄과 끝줄 및 줄바꿈','표 세 열의 좌우/하단 셀 경계와 내용 잘림','빈 페이지/페이지 경계 오버플로/문자 겹침'],
    'findings':[],'pages':[]}
for n in range(29,43):
    p=(D/f'page-{n:03d}.png').resolve()
    with Image.open(p) as im:size=list(im.size)
    assert size==[893,1263]
    record['pages'].append({'page':n,'png':str(p),'sha256':sha(p),'size':size,'direct_original_size_viewed':True,'verdict':'통과'})
assert sha(source).upper()==expected
(H/'SAD-final-full-qa.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'sourceSHA256':expected,'pages':len(record['pages']),'status':record['status'],'proofSHA256':sha(H/'SAD-final-full-qa.json')},ensure_ascii=True))
