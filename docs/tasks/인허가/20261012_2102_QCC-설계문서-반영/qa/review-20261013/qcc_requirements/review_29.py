import hashlib, json, pathlib, re

R = pathlib.Path(__file__).parent
guideline = pathlib.Path('E:/workspace/projects/sound1-fw-e8300-backup/docs/참고/인허가/사운드1/SW 밸리데이션 문서 작성·검증 지침.md')
titles = dict((int(n), title) for n, title in re.findall(r'^### (\d+)\. (.+)$', guideline.read_text('utf-8'), re.M))
assert len(titles) == 29
contract = json.loads((R / 'requirements-contract.json').read_text('utf-8'))
qa = json.loads((R / 'srs-qa.json').read_text('utf-8'))
notes = json.loads((R / 'full-note-qa.json').read_text('utf-8'))
style = json.loads((R / 'srs-comparison-qa.json').read_text('utf-8'))
states = {
1: ('통과', 'SRS F/G/H/I, SR 4/5장', '같은 Bluetooth 송수신은 기존69/FW15로 통합했다. 82는 삭제 비교 이력만 보존했다. 83은 실제 ACDOC17/18/20의 사용 설명서 요구이며 기존81에 같은 기능이 없어 유지했다.', ['srs-qa.json', 'requirements-contract.json']),
2: ('통과', 'SRS 82개 현재 F/H, 실제 매트릭스 100~149 및348/349/351행', '실제 상위 추적이 있는50요구는 첫 기본 블록의 원문과 다음줄 상세 분리까지 일치한다. 원본H빈32요구는 매트릭스 대조 분모에서 제외하며 상위ID를 추정하지 않았다.', ['srs-comparison-qa.json', 'matrix-readonly.xlsx']),
3: ('미실행', '전체 SRS→SAD→SDDD→시험 연결', '본인 소유3문서의 현재 식별ID/실제 상위 요구 연결은 통과했다. 최종 설계3개 및 시험6개의 전체 downstream 독립 대조는 해당 담당자/루트의 검사 결과에 연결해야 한다.', ['requirements-contract.json']),
4: ('통과', 'QCC SRS 실제 원문 및 현행3문서', 'QCC 실제본문을 읽고 스마트케이스/CC를 외부기QCC의 미갱신표기로 해석했다. 본문 없는 충전 항목은 신규요구로 만들지 않았다. 원문ID대응은 로컬 근거에만 남겼다.', ['inspection.json', 'requirements-contract.json']),
5: ('통과', 'SR 2장/그림2, SGR 5.1.1, SRS15/69', '사용자 확정 사실 DMIC2→E8300 및QCC를 반영했고 통화 상향과 QCC→E8300 I2S하향을 구분했다. CFX/HEAR/CM3와 칩/장치 경계를 보존하며 QCC핀/클록을 만들지 않았다.', ['SR-authored.json', 'SGR-authored.json']),
6: ('통과', 'SRS15/47/48/49/52/54/69/71/72/76/78/83 및 연결 문서', '설계/시험 담당과 최종문구를 공유하고 키파생/인증/링크보호, 180초/30초/60초 조건과 DMIC2 피드백을 앞 문서에 재반영했다. native에서 찾은 서식 결함은 표적 수정 후 다시 검사한다.', ['requirements-contract.json', 'srs-layout-fix2.json', 'srs-layout-fix3.json']),
7: ('통과', 'SR/SGR 전체본문, SRS 현재F', '기존 E8300 절/설명 수준 안에 QCC기능을 통합했다. 제공자료/병합과정 문구는 본문에서 제거했고 요구와 상세를 같은 항목 안에 배치했다. 이번 상세/미비안내의 가운데점 0개를 확인했다. 매트릭스 기본문구/기존 문장은 유지했다.', ['SR-authored.json', 'SGR-authored.json', 'srs-qa.json', 'new-sentence-punctuation-qa.json']),
8: ('해당없음', '본인 소유3문서', '상세설계 SDDD는 설계 담당 소유이다. 본인 문서는 요구/일반요구이며 유닛설계 목적/I/O/처리 재구성 대상이 없다.', []),
9: ('해당없음', 'SRS/SR/SGR', '소유3문서에는 I/A/D/T 방법 열 또는 방법 정의 표가 없다. 기준서의 약어/정의는 시험 담당 검사 대상이다.', []),
10: ('해당없음', 'SRS/SR/SGR', '시험 방법 분류 열이 없다. 송신 SYS82는 기존69/FW15에 연결하며 정량범위 없는 시연 D라는 시험 담당/루트 결정을 공유했다.', []),
11: ('해당없음', '본인 소유3문서', '실행 준비/절차를 작성하는 시험 기준서 소유권이 없다. 요구의 미확정 조건은 실제 현재 미비안내에 남겼다.', []),
12: ('해당없음', '본인 소유3문서', '기준서/보고서 공통 열·행·병합 대조는 시험 담당의6Excel에 적용한다.', []),
13: ('해당없음', '본인 소유3문서', '실제 시험 결과값을 추가하는 보고서 소유권이 없다. 요구사항에 시험 결과를 만들지 않았다.', []),
14: ('해당없음', '본인 소유3문서', 'RTT 예상/관찰 문자열 기입 문서가 아니다.', []),
15: ('해당없음', '본인 소유3문서', '조건/검증 번호가 있는 시험 행이 없다.', []),
16: ('해당없음', '본인 소유3문서', '시험 결과 칸이 없다. 제품 시험 결과는 새로 생성하지 않았다.', []),
17: ('해당없음', '본인 소유3문서', '시험자 확인 결과 또는 수집로그를 새로 기입하는 문서가 아니다.', []),
18: ('통과', 'SR 5장 검증 질문/6장 위험분석 재평가', '과거 예 답변을 신규QCC 구현·정량시험 완료로 확대하지 않는다. 현재 미확정 조건/시험은 판정 대기로 설명하고 위험 영향 재평가를 남겼다.', ['SR-authored.json']),
19: ('해당없음', '본인 소유3문서', '시험자 확인 결과의 중간값/음수 절삭을 적용한 결과값 기입이 없다.', []),
20: ('해당없음', 'SR 그림2/기존 그림', '하드웨어 구성도를 수정했고 파형/LED예상도식 또는 계측 캡처를 생성하지 않았다. 구조 그림은 실측 화면 주장이 없다.', ['SR-authored.json']),
21: ('해당없음', '본인 소유3문서', '일반 시간 시험 입력 날짜가 없다. 작성/서명/승인 이력을 시험날짜로 치환하지 않았다.', []),
22: ('통과', 'SRS 현재18안내/SR5안내/SGR2안내', '접두어와 전체 설명의 기본10+2pt, 지정색/밑줄을 실제현재 richrun/Wordrun 전체에서 검사했다. DMIC2연결은 확인됨으로 적고 핀/클록/동시조건 등 실제 남은 자료를 구분했다. 보존Q접두어는 임의 해제하지 않았다.', ['full-note-qa.json']),
23: ('미실행', '중앙 QCC 위치 목록', '본인에게 MD수정 권한이 없다. 저장된현재ID/행좌표/미비안내 전체 문구를 루트에 전달했으며 목록 최종 대조는 루트 책임이다.', ['full-note-qa.json', 'srs-qa.json']),
24: ('통과', 'SRS B3:I107 전체 실제 인쇄범위 29쪽', '최신A568 실제Excel native29쪽 모두 원본 크기로 확인했다. 긴F상세와H상위추적, C18/D26분류, G/H60 RID47/FW22 및 과거I문구의 잘림이 해소됐다. 같은내용 분할 외곽테두리와 내부경계,409.5pt높이상한을 확인했다. 원본B열 IEC분류/NA와96~107행 규제요구는 보존했다.', ['srs-layout-fix2.json', 'srs-layout-fix3.json', 'native-final-qa.json', 'srs-comparison-qa.json']),
25: ('통과', 'SRS 모든 비교richrun', '원문빨강[-]/다음수정초록[+]순서와 전체문자색을 검사했다. 현재미비안내만 지정색예외이다. 요구/ID/추적의 실질차이를 본문에 보존하며 현재값은[+]로 분리했다.', ['srs-comparison-qa.json']),
26: ('통과', 'SR/SGR Word본문·그림', '김은수 실제 삽입/삭제/속성추적과 기존 이력을 보존했다. 새 변경 거부는 시작before현재본문을 복원한다. 최신G와같은 native필드정규화 후 수락/거부본문·그림/저자일자 일반revision/안내 4개 비교가 통과했다. Word media파트 재번호는 실제bytes/hash 및 수락/거부 그림 순서 보존으로 확인했다.', ['SR-authored.json', 'SGR-authored.json', '../SR-layout-fields-comparison.json', '../SGR-layout-fields-comparison.json', 'native-final-qa.json']),
27: ('통과', 'G 설계입력 외부기3사본/보호원본23개', 'G기존단계동일경로 [E8300-QCC]3사본만 수정했고 E원본/Q원문23개 SHA256는 시작기준과동일하다. 표지/서명/검토/승인빈칸과 기존개정이력은 보존했다.', ['srs-comparison-qa.json', 'SR-authored.json', 'SGR-authored.json']),
28: ('통과', '저장현재G SR13/SGR12/SRS29쪽 전체native', '82현재요구의F/H/I실제문구,50매트릭스 기본블록과32원본하위요구를 구분해 대조했다. 최신native 전체54쪽을 직접 확인했고 Word25쪽은 검토후 게시본과pixel동일이다. 표기준선/SGR표제목 고립/Excel경계 잘림은 수정후 해소됐다. 제품시험 또는 새승인은 생성하지 않았다.', ['srs-qa.json', 'full-note-qa.json', 'native-final-qa.json', 'SR-final-layout.json', 'SGR-final-layout.json']),
29: ('통과', 'SR/SGR 모든본문/표/캡션', '실제저장59개 TDC스타일과지원스타일 참조/기본1.25배 및뒤8pt를 확인했다. 전체native에서 본문10pt, 표기준선/너비/정렬/머리행반복과캡션/SEQ/목차필드를 확인했다. SR/SGR 표외곽6px이동 및 SGR표준및규제제목의5쪽표연결은 최종저장PNG에서 정상이다. 원본PAGE재시작과이력보존. 유닛목적/I/O/처리 표는 이 소유문서에 해당없다.', ['SR-tdc-style-fix.json', 'SGR-tdc-style-fix.json', 'SR-final-layout.json', 'SGR-final-layout.json', 'native-final-qa.json'])
}
rows = [{'number': n, 'title': titles[n], 'status': states[n][0], 'location': states[n][1], 'evidence_and_scope': states[n][2], 'evidence_files': states[n][3]} for n in range(1, 30)]
out = {'scope': '소유 SRS/SR/SGR3문서에 대한29원칙 검사. 다른 소유자의 설계/시험/중앙MD검사를 대신하지 않는다.',
       'guideline_sha256': hashlib.sha256(guideline.read_bytes()).hexdigest().upper(),
       'active_requirement_count': qa['active_count'], 'matrix_equality_denominator': 50, 'matrix_unregistered_original_subordinate_count': 32,
       'checks': rows, 'current_hashes': {k: v['sha256'] for k, v in notes.items()},
       'native_status': '저장현재G SR13/SGR12/SRS29쪽 전체native 직접검사 통과. Word검토후 게시본 전체PNG pixel동일. 전체downstream/중앙목록은 루트 최종증거에 연결한다.',
       'product_execution': '이번 문서 작업에서 제품 시험을 새로 실행하지 않았다. 자료/설계 반영과 시험 통과/승인을 구분한다.'}
(R / 'review-29.json').write_text(json.dumps(out, ensure_ascii=False, indent=2), 'utf-8')
print(json.dumps({'checks': len(rows), 'statuses': {s: sum(x['status'] == s for x in rows) for s in ['통과', '실패', '미실행', '해당없음']}}, ensure_ascii=False))
