from pathlib import Path
import json, hashlib, datetime

h = Path(__file__).resolve().parent
q = h.parent
def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()

prior = h / 'independent-unit-native-qa.json'
independent = json.loads(prior.read_text(encoding='utf-8'))['latest_layout_render_link']
documents = []
for code, count in [('SUVC-main', 305), ('SUVR-main', 260)]:
    folder = q / 'native-excel/tests' / code
    pages_path = folder / 'pages.json'
    pages = json.loads(pages_path.read_text(encoding='utf-8'))
    assert len(pages) == count and [x['page'] for x in pages] == list(range(1, count + 1))
    source = next(x for x in independent['sources'] if x['code'] == code)
    current_hash = sha(source['path'])
    assert current_hash == source['sha256_after']
    contacts = []
    for first in range(1, count + 1, 12):
        last = min(first + 11, count)
        image = folder / f'contact-{first:03}-{last:03}.png'
        contacts.append({'first_page': first, 'last_page': last, 'image': str(image), 'sha256': sha(image),
            'directly_viewed_contact_at_original_image_size': True,
            'scope': '각 페이지 축소 타일의 전체 표 배치, 머리행, 큰 빈 페이지, 좌우 인쇄 범위 누락을 육안 확인. 세부 글자 및 모든 문구의 full-size 검사는 별도 증거에 한정.',
            'findings': [], 'verdict': '통과(축소 모음 레이아웃 범위)'})
    document = {'code': code, 'source': source['path'], 'sourceSHA256': current_hash, 'pages_json': str(pages_path),
        'pages_jsonSHA256': sha(pages_path), 'actual_page_count': count,
        'contacts': contacts,
        'page_png_sha256': [{'page': n, 'sha256': sha(folder / f'page-{n:03}.png')} for n in range(1, count + 1)],
        'sourceSHA256_after_readonly_QA': sha(source['path'])}
    assert document['sourceSHA256'] == document['sourceSHA256_after_readonly_QA']
    documents.append(document)

out = {'checkedAt': datetime.datetime.now().astimezone().isoformat(), 'documents': documents,
    'reviewed_contacts': 48, 'covered_thumbnail_pages': 565,
    'findings': [],
    'normal_layout_observation': '긴 RTT 문단은 기준/결과 칸에서 다음 페이지로 이어지고 시험 그룹 시작을 고정한 부분 페이지의 아래 공백이 있다. 전체 비어 있는 페이지나 표의 좌우 누락은 축소 모음에서 발견하지 않았다.',
    'independent_full_size_review': {'evidence': str(prior), 'sha256': sha(prior), 'previous_scope_mapped': 44, 'additional_current_direct_page': 1, 'current_full_size_scope': 45},
    'scope_limit': '48개 contact로 565페이지를 축소 레이아웃 확인한 결과이다. 모든 565페이지의 글자 단위 잘림, 모든 시험 내용 또는 제품 시험 통과를 주장하지 않는다. 직접 full-size 검토한 범위는 연결된 별도 QA의 45페이지이다.',
    'status': '통과(전체 축소 모음 레이아웃 범위); QCC 제품 미실행/판정 대기 보존'}
p = h / 'unit-contact-overview-qa.json'
p.write_text(json.dumps(out, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'path': str(p), 'sha256': sha(p), 'contacts': 48, 'thumbnail_pages': 565, 'findings': []}, ensure_ascii=False))
