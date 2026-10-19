"""Align the two system workbooks without changing test identities or results.

Artifact Tool owns cell authoring. This script snapshots, plans, conservatively
merges authored values into the original OOXML, and verifies the edit boundary.
"""
from copy import deepcopy
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import re
import shutil
import sys
from lxml import etree as E
from review_srs_materials import TASK, ROOT, DHF, S, NS, find_file, workbook

QA = TASK / 'qa/system-upper-method-alignment-20261012'
q = lambda n: '{' + S + '}' + n
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()


def dump(name, value):
    (QA / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf8')


def package(path):
    with ZipFile(path) as z:
        return {n: z.read(n) for n in z.namelist()}


def cell(root, address):
    return root.find(f'.//s:c[@r="{address}"]', NS)


def snapshot():
    QA.mkdir(parents=True, exist_ok=True)
    assert not (QA / 'baseline.json').exists(), 'Keep the captured baseline'
    paths = {code: find_file(folder, code) for code, folder in
             [('SSTC', '03_설계 출력'), ('SSTR', '04_설계 검증')]}
    evidence = {'paths': {k: str(v) for k, v in paths.items()},
                'hashes': {k: sha(v) for k, v in paths.items()},
                'books': {k: workbook(v) for k, v in paths.items()}}
    for code, path in paths.items():
        shutil.copyfile(path, QA / (code.lower() + '-before.xlsx'))
    source = TASK / 'qa/srs-remove-six-notices-matrix-review-20261012/matrix-current-readonly.xlsx'
    evidence['matrix_sha256'] = sha(source)
    evidence['matrix'] = workbook(source)
    evidence['SRS'] = workbook(find_file('02_설계 입력', 'SRS'))
    evidence['SUVR'] = workbook(find_file('04_설계 검증', 'SUVR'))
    evidence['protected'] = {str(p): sha(p) for p in DHF.rglob('*')
                             if p.is_file() and p not in paths.values() and not p.name.startswith('~$')}
    evidence['code'] = {str(p): sha(p) for folder in ['src', 'tests']
                        for p in (ROOT / folder).rglob('*') if p.is_file()
                        and p.suffix.lower() in ['.c', '.h', '.hct', '.py', '.md']}
    dump('baseline.json', evidence)
    print('Captured two targets,', len(evidence['protected']), 'protected documents,',
          len(evidence['code']), 'code/test files')


def inspect():
    evidence = json.loads((QA / 'baseline.json').read_text(encoding='utf8'))
    t = evidence['books']['SSTR']['보고서_시스템 시험']['cells']
    starts = sorted((int(a[1:]), int(v.strip().rsplit('-', 1)[1])) for a, v in t.items()
                    if a.startswith('E') and re.fullmatch(r'SW-FW-RID-\d+', v.strip()))
    wanted = {int(x) for x in sys.argv[2:]} if len(sys.argv) > 2 else {1, 2, 5, 6, 8, 9, 11, 12, 53, 56, 57, 58, 59, 61, 62, 65, 75, 79}
    for i, (row, rid) in enumerate(starts):
        if rid not in wanted:
            continue
        end = starts[i + 1][0] if i + 1 < len(starts) else 282
        print(f'=== TEST {rid} / rows {row}:{end-1} ===')
        for col in 'DGH IJKL'.replace(' ', ''):
            for r in range(row, end):
                if t.get(col + str(r), '').strip():
                    print(col + str(r), t[col + str(r)].strip())


def units():
    evidence = json.loads((QA / 'baseline.json').read_text(encoding='utf8'))
    t = evidence['SUVR']['보고서_유닛 시험']['cells']
    words = sys.argv[2:]
    for a, v in t.items():
        if a.startswith('D') and re.fullmatch(r'\s*SW-FW-UnitTest-[A-Z0-9]+-ID-\d+\s*', v):
            row = int(a[1:])
            description = t.get('E' + str(row), '')
            if not any(w in v or w in description for w in words):
                continue
            print('\n===', v.strip(), description.strip(), '===')
            for col in ['I', 'J', 'K', 'L']:
                for r in range(row, min(row + 10, 1735)):
                    if t.get('D'+str(r), '').strip() and r != row:
                        break
                    if t.get(col + str(r), '').strip():
                        print(col + str(r), t[col + str(r)].strip()[:5000])


def plan():
    evidence = json.loads((QA / 'baseline.json').read_text(encoding='utf8'))
    t = evidence['books']['SSTR']['보고서_시스템 시험']['cells']
    common = {}
    notes = {}
    issues = []

    def set_value(address, text, test, reason):
        common[address] = {'after': '\n' + text.strip() + '\n', 'test': test, 'reason': reason}

    def append(address, text, test, reason):
        set_value(address, t.get(address, '').strip() + '\n\n' + text, test, reason)

    def pending(address, text, test):
        notes[address] = {'after': '\n' + t[address].strip() + '\n\n[E8300 보완] ' + text + '\n',
                          'test': test, 'reason': '새 조건의 측정값을 기존 기록으로 대신하지 않음',
                          'highlight_prefix': '[E8300 보완]'}

    append('I34', '전체 자극률은 같은 시간 동안 모든 활성 채널에서 나온 펄스 수를 합하여 구한다. 채널 하나의 반복률과 구분한다. 채널별 조건은 시험 2에서 확인한다.', 1, '상위 2의 전체/채널별 자극률 구분')
    set_value('H35', '펄스폭: 13 us\n채널을 구분할 수 있는 CIS 맵이 적용되어야 한다.\n24채널과 32채널 조건을 구분한다.', 2, '상위 2의 32채널 조건 연결')
    append('I35', '24채널 조건의 채널별 반복률을 먼저 확인한다. 32채널 조건은 같은 시험에서 따로 확인한다. 전체 자극률은 시험 1의 측정값과 대조한다.', 2, '같은 시험 안의 비교 조건 보완')
    append('J37', '32채널 조건: 전극 1~32 모두 활성, 펄스폭 13 us.\n채널별 자극률: 750 Hz, 허용오차 ±5%.\n모든 채널이 712.5~787.5 Hz이어야 한다.\n24×1,000과 32×750은 모두 전체 24,000회/s이다.', 2, '상위 2의 32채널/750Hz 및 24채널/1000Hz')
    pending('K37', '기존 기록은 24채널/1,000 Hz 조건이다. 32채널/750 Hz 조건의 채널별 파형과 측정값을 이곳에 연결한다.', 2)
    append('H41', '음압에 따른 전류 범위 확인에는 같은 채널의 T/C 레벨, 펄스폭, 오디오 볼륨과 자극 볼륨을 고정한다. 묵음 처리는 해제한다.', 5, '상위 5의 음성 입력에 따른 T/C 전류 범위')
    append('I41', '같은 채널에 낮은 음압과 높은 음압을 입력하여 T/C 범위의 전류를 확인한다. 입력 음압/ADC 조건은 필수성능시험 기준서의 해당 전류 시험 조건을 사용한다.', 5, '기존 필수성능 측정 조건의 근거 연결')
    append('J43', '음압을 바꾼 경우의 비교 기준:\nT=500 uA, C=1,500 uA인 동일 채널에서 낮은 입력의 T 출력은 450~550 uA, 높은 입력의 C 출력은 1,350~1,650 uA이어야 한다.\n중간 입력의 계산 전류는 같은 맵의 T~C 사이이고, 실측값은 그 계산값의 ±10%이어야 한다.\n시험 57의 필수성능시험 결과에서 같은 입력/맵 조건의 전류 기록을 연결한다.', 5, '기존 고정 전류 시험과 음압 응답 근거 구분')
    pending('K43', '500/1,000/1,500 uA의 기존 설정 전류 기록은 유지한다. 음압 입력에 따른 T/C 출력은 시험 57에 연결된 필수성능시험 보고서의 입력/맵/전류 기록으로 확인한다.', 5)
    append('I44', '최소 폭 13 us 다음의 14 us 설정을 확인하여 1 us 단위 변경 여부도 대조한다. 기존 25/100/255 us 조건은 유지한다.', 6, '상위 3의 1us 단위와 범위 끝값')
    append('J44', '1 us 증가 조건: 설정 펄스폭 14 us.\n음극/양극 폭이 각각 13.30~14.70 us이어야 한다.\n13→14 us 설정 차이는 1 us이며 255 us까지 같은 단위로 설정할 수 있어야 한다.', 6, '13/14/255us 검증 조건 명시')
    pending('K44', '기존 13 us 결과와 구분하여 14 us의 음극/양극 폭 측정값을 연결한다. 설정값의 1 us 차이만으로 실제 펄스폭을 판정하지 않는다.', 6)
    append('G50', '자극 출력 비교에는 정상 내부기와 내부기 출력 측정용 지그 또는 32채널 핀 헤더 내부기 테스트 지그를 사용한다.', 8, 'OD 청취와 내부기 자극 출력 구분')
    set_value('I50', '같은 음원을 재생한 채 오디오 볼륨을 1부터 10까지 차례로 바꾸고 조회값과 내부기 자극 출력의 변화를 확인한다.\nOD1 출력은 같은 음원의 크기 변화를 듣는 보조 확인에 사용한다.', 8, '상위 6의 10단계 및 내부기 출력')
    append('J50', '볼륨 1~4 확인: 각 설정의 조회값이 설정한 단계와 같고, 같은 음압/맵에서 AGC 설정에 따른 자극 출력 변화가 확인되어야 한다.', 8, '기존 3행 안에서 전체 10단계 구분')
    append('J51', '볼륨 5~7 확인: 각 조회값이 설정한 단계와 같고, 같은 음압/맵에서 자극 출력 변화가 확인되어야 한다.', 8, '기존 3행 안에서 전체 10단계 구분')
    append('J52', '볼륨 8~10 확인: 각 조회값이 설정한 단계와 같고, 같은 음압/맵에서 자극 출력 변화가 확인되어야 한다.\nAGC 유닛의 볼륨 1/10 입력/이득 결과와 시험 25의 볼륨 제어 결과를 보조 근거로 대조한다.', 8, '기존 AGC/볼륨 제어 증거 연결')
    pending('K52', '기존 기록은 볼륨 1/5/10의 OD 청취 결과이다. 나머지 단계의 조회값과 내부기 자극 출력 확인 기록을 연결한다. OD 청취를 전극 출력 측정으로 바꾸지 않는다.', 8)
    append('I53', '소프트웨어 유닛 검증 보고서의 HEAR FFT 입력/출력과 완료 이벤트 결과를 함께 대조한다. 같은 입력의 최대 주파수 성분이 CFX 대역 분석에 전달되는지 확인한다.', 9, '상위 7의 입력/출력 벡터 증거 연결')
    append('J53', 'FFT 입출력 비교 근거:\n512개 영 입력 → FFT 512개/크기 256개 모두 0.\n진폭 65,536의 1,000 Hz/2,000 Hz 정현파 → 최대 bin 32/64.\n512개 모두 32,768인 입력 → DC 성분.\n+32,768/-32,768 교대 입력 → Nyquist 성분.\n각 입력의 완료 이벤트=1. 수치 오차/블록 지수는 해당 유닛 검증 기준으로 비교한다.', 9, '기존 5개 HEAR 벡터 재사용')
    # These are already recorded unit results, not a new system-board run.
    notes['K53'] = {'after': '\n' + t['K53'].strip() + '\n\n추가 근거: 소프트웨어 유닛 검증 보고서의 HEAR-ID-2 결과.\n영 입력의 FFT/크기 배열=모두 0.\n1,000 Hz: 최대 bin=32, 환산 크기=8,372,210.\n2,000 Hz: 최대 bin=64, 환산 크기=8,372,226.\nDC/Nyquist 입력: 최대 bin=0, 환산 크기=8,372,222.\n각 완료 이벤트=1.\n', 'test': 9, 'reason': '이미 기록된 유닛 결과를 출처/범위와 함께 재사용'}
    append('H64', 'CIS 순서 확인과 인접 전극 연속 자극 회피를 구분한다. 기본 CIS 맵은 1~32 순서이므로 비인접 순서 확인에는 별도로 설정한 CIS 순서가 필요하다.', 11, '상위 9와 현재 기본 CIS 맵의 차이 보존')
    append('J64', '상위의 비인접 순서 확인:\n같은 전극 배치에서 맵에 정의한 CIS 순서를 출력해야 하며 연속한 전극 번호의 차이가 1이면 안 된다. 한 주기의 마지막과 다음 주기의 첫 전극도 확인한다.\n기본 1~32 순서와 일치한다는 결과는 순서 전달의 근거이고 비인접 조건의 근거는 아니다.', 11, '기존 순서 기대값을 임의 교체하지 않음')
    pending('K64', '기존 결과는 1~32의 CIS 순서 전달이다. 비인접 순서가 설정된 맵과 그 출력 순서 기록은 별도로 연결해야 한다. 기본 맵/코드는 이번에 바꾸지 않았다.', 11)
    append('I68', '같은 블록의 처리 완료 시각도 시험 58의 기록과 연결해 처리에 걸린 시간을 확인한다.', 12, '상위 10의 주기와 완료 시간 구분')
    append('J69', '처리 완료 시간의 근거는 시험 58에서 확인한다.\n완료 시각−시작 시각은 1.00 ms 이하이고 다음 블록 시작 전에 완료되어야 한다. 시작 간격만으로 완료 시간을 판정하지 않는다.', 12, '완료≤1ms 조건의 기존 시간 시험 연결')
    append('I203', '고객불만처리 절차 또는 A/S 처리 방침에서 기존 제품 회수와 새 제품 교환 내용을 확인한다.', 53, '상위 30의 고객불만/A/S 방침')
    append('J203', '고객불만처리/A/S 방침에 기존 제품을 회수하고 새 제품으로 교환한다는 내용이 있어야 한다. 취약점 확인/업데이트 절차와 구분하여 문서 이름과 해당 항목을 연결한다.', 53, '업데이트 절차만으로 상위 교환 기준을 판정하지 않음')
    pending('K203', '기존 기록은 취약점 확인/업데이트 절차 확인이다. 제품 회수/새 제품 교환이 명시된 고객불만처리 또는 A/S 문서의 해당 항목을 연결한다.', 53)
    append('H239', '자극 크기 증가의 최소 시간이 승인된 설계 조건으로 정해져 있어야 한다. 최종 크기 계산값과 실제 증가 시간을 구분한다.', 56, '상위 47의 최소 증가 시간')
    append('I239', '변경 요청부터 최종 크기 도달까지 걸린 시간을 같은 파형에서 구하여 정해진 최소 증가 시간과 비교한다.', 56, '기존 파형에서 시간 판정 연결')
    append('J239', '최종 크기 도달 시간은 정해진 최소 증가 시간보다 짧으면 안 된다.\n[E8300 보완] 최소 증가 시간의 수치/단위와 설계 근거를 입력한다. 1→4의 최종 Cnew 계산값만으로 이 시간을 판정하지 않는다.', 56, '확인되지 않은 최소 시간을 임의 생성하지 않음')
    pending('K239', '단계적 증가를 확인한 기존 결과는 유지한다. 최소 증가 시간의 수치와 요청/최종 도달 시각이 연결된 기록을 추가한다.', 56)
    append('I243', '필수성능 요구사항이 소프트웨어 요구사항 사양서와 설계 출력에 연결되고, 같은 조건으로 시험되었는지도 확인한다.', 57, '상위 39/40의 설계 반영 및 시험 수행 연결')
    append('J243', '상위 필수성능 요구사항 → 소프트웨어 요구사항 → 설계 항목 → 필수성능 시험 항목/결과가 빠짐없이 연결되어야 한다.\n별도 시험을 새로 만들지 않고 같은 제품/맵/입력 조건의 기존 보고서 측정값과 판정을 사용한다.', 57, '기존 필수성능 기록의 조건 일치')
    append('I244', '처리 완료 시각에서 시작 시각을 빼 실제 처리 시간을 구한다. 해당 값과 시작 간격은 따로 비교한다.', 58, '상위 41의 수행 시간 측정')
    append('J245', '각 블록의 완료−시작 시간도 1.00 ms 이하이어야 한다. 시작 주기의 ±5% 허용을 완료 시간 상한에 더하지 않는다.', 58, '상위 10/41의 정량 완료 조건')
    pending('K245', '기존 기록은 완료가 다음 시작보다 앞섰다는 내용이다. 같은 시각 기록에서 구한 블록별 처리 시간과 최대값을 연결하여 1.00 ms 이하인지 확인한다.', 58)
    set_value('I247', '정상 데이터, 범위 밖 데이터, 필수 데이터가 빠진 명령을 각각 보내 응답과 설정 유지 여부를 확인한다.\n0x45 명령의 방향 1은 증가, 2는 감소이다. 각 정상 확인은 시작 자극 볼륨 2로 준비한다.', 59, '상위 28의 정상/범위 밖 응답')
    append('J247', '정상 데이터 확인: 시작 자극 볼륨 2에서 [0x45,0x01] → 정상 응답/볼륨 3, 같은 시작값에서 [0x45,0x02] → 정상 응답/볼륨 1.\n명령 처리 유닛/시험 31의 자극 볼륨 결과를 함께 대조한다.', 59, '정상 입력 벡터와 응답 의미')
    append('J248', '범위 밖 확인: 시작 자극 볼륨 2에서 [0x45,0x00] 및 [0x45,0x03] → 데이터 범위 오류 응답, 볼륨 2 유지.\n오류 코드/응답 형식은 같은 통신 규칙과 대조한다.', 59, '길이 누락과 값 범위 오류 구분')
    pending('K248', '기존 기록은 필수 데이터 누락 응답이다. 정상 0x01/0x02와 범위 밖 0x00/0x03의 응답/볼륨 결과는 기존 명령 처리 기록과 연결한다.', 59)
    set_value('H260', '내부기 미연결, 배터리 잔량 80% 이상. 오류/OTA/매핑/페어링 표시와 전원 켜짐 표시가 없어야 한다.', 61, '녹색 준비 상태와 다른 배터리 상태를 혼합하지 않음')
    set_value('I260', '내부기 미연결, 잔량 80% 이상에서 정상 사용 준비 LED를 확인한다. 낮은 잔량의 색/점멸은 시험 22와 62에서 확인한다.', 61, '상위 43의 정상 준비 조건 고정')
    append('H261', '이 시험은 내부기 미연결의 충전/잔량 표시를 확인한다. 내부기 정상 연결 시의 흰색 사용 표시는 시험 20과 구분한다.', 62, '상위 44와 사용/배터리 상태 구분')
    append('J261', '상위 기준 대조: 상위 문서는 충전기 연결 시 흰색을 요구한다. 이 시험의 현재 미연결 조건은 충전 여부와 관계없이 잔량별 색을 표시한다. 두 조건의 적용 범위를 확인하며 이번에 기존 색상 기준을 임의로 바꾸지 않는다.', 62, '상위/현재 색상 충돌을 숨기지 않음')
    append('I274', '정상 연결의 흰색은 시험 20, 미연결 준비의 녹색은 시험 61, 잔량에 따른 주의 표시는 시험 22/62와 함께 대조한다.', 65, '상위 46의 정상/주의/오류 상태 연결')
    append('J275', '상위 기준 대조: 상위의 주의 색상은 노란색이다. 기존 결과의 주황색과 적용 상태/제품 표시 색상이 같은지 확인한다. 이번에 기존 색상/결과를 서로 바꾸지 않는다.', 65, '주의 색상 차이의 확인 필요 보존')
    append('I219', '무선 응답의 오류 코드와 앱의 안내 문구를 구분한다. 키/암호/보호 원문 노출 여부를 함께 확인한다.', 75, '상위 29의 오류 메시지 적용 경계')
    append('J221', '무선 오류 응답은 정의된 오류 코드/필드로 전달되어야 한다. 보호 대상 원문을 설명하는 텍스트가 응답에 포함되면 안 된다.\n상위의 “텍스트 오류 메시지 없음”은 앱의 사용자 안내까지 포함하는지 적용 범위를 확인한다.', 75, '기존 앱 오류 표시와 상위 텍스트 기준의 구분')
    append('J233', '로컬 로그 조회: 등록 내부기 정보가 일치하고 정상 연결된 경우만 허용한다. 미연결/정보 불일치/연결 해제 시 기록 읽기 0회, 기록 출력 0건이어야 한다.\n소프트웨어 유닛 검증 보고서의 CM3-ID-5 조회 조건/결과를 연결한다.', 79, '사용자 확인된 기존 로그 조회 유닛 결과 연결')
    # The QCC note must not require QCC evidence for the E8300 local gate.
    old = t['K233'].strip()
    body = old.split('(QCC 검증 결과 필요)', 1)[0].rstrip()
    notes['K233'] = {'after': '\n' + body + '\n\n추가 근거: 유닛 검증 보고서 CM3-ID-5. 등록 정보 일치/정상 연결이면 저장 기록 조회, 미연결/불일치/해제이면 읽기 0회/출력 0건.\n\n[QCC 자료 입력] QCC를 통한 외부 로그 읽기/전송을 제공하는 경우, 인가/비인가 읽기, 전송 보호와 원격 편집 부재의 자료를 입력한다. E8300 로컬 RTT 조회 조건과 구분한다.\n', 'test': 79, 'reason': '로컬 E8300 근거와 조건부 QCC 외부 전송 자료 분리', 'highlight_prefix': '[QCC 자료 입력]'}
    issues = [
        {'tests': [11], 'upper': 9, 'reason': '기본 CIS 맵 1~32는 인접 전극 연속 순서. 비인접 시험 맵/기록 미연결'},
        {'tests': [56], 'upper': 47, 'reason': '최소 증가 시간의 승인 수치/단위를 확인하지 못함'},
        {'tests': [62, 65], 'upper': [44, 46], 'reason': '충전 흰색 및 주의 노란색의 상위 기준과 기존 색상/상태가 다름'},
        {'tests': [75], 'upper': 29, 'reason': '상위 텍스트 오류 메시지 없음의 무선 응답/앱 안내 적용 범위 미확정'},
    ]
    # Keep previous pass decisions, but do not silently apply them to added scope.
    for addr, test, meaning in [('L35', 2, '32채널 조건 기록'), ('L41', 5, '음압/전류 기록 연결'),
                                ('L44', 6, '14 us 기록'), ('L50', 8, '10단계/내부기 출력 기록'),
                                ('L64', 11, '상위 비인접 조건'), ('L203', 53, '회수/교환 방침 연결'),
                                ('L239', 56, '최소 증가 시간 근거'), ('L244', 58, '처리 시간 수치'),
                                ('L247', 59, '정상/범위 밖 응답 기록'), ('L261', 62, '상위 충전 색상 적용 범위'),
                                ('L274', 65, '상위 주의 색상 적용 범위'), ('L219', 75, '상위 오류 문구 적용 범위')]:
        notes[addr] = {'after': '\n기존 결과 통과\n' + meaning + ' 확인 대기\n',
                       'test': test, 'reason': '기존 통과와 추가 조건/근거의 확인 상태를 구분'}
    plans = []
    for code in ['SSTC', 'SSTR']:
        sheets = evidence['books'][code]
        main = '기준서_시스템 시험' if code == 'SSTC' else '보고서_시스템 시험'
        edits = {}
        for sheet, data in sheets.items():
            for addr, before in data['cells'].items():
                if 'SYS-RID-FW-' in before:
                    edits[(sheet, addr)] = {'sheet': sheet, 'cell': addr, 'before': before,
                        'after': before.replace('SYS-RID-FW-', 'SYS-RID-SP-FW-'),
                        'reason': '상위 매트릭스의 외부장치 펌웨어 식별 형식', 'test': None}
        for addr, value in common.items():
            before = sheets[main]['cells'].get(addr, '')
            if code == 'SSTC':
                assert before == t.get(addr, ''), ('Common baseline differs', addr)
            edits[(main, addr)] = {'sheet': main, 'cell': addr, 'before': before, **value}
        if code == 'SSTR':
            for addr, value in notes.items():
                edits[(main, addr)] = {'sheet': main, 'cell': addr,
                                      'before': sheets[main]['cells'].get(addr, ''), **value}
        plans.append({'code': code, 'target': evidence['paths'][code],
                      'sha256': evidence['hashes'][code], 'main': main,
                      'before': str(QA / (code.lower() + '-before.xlsx')),
                      'artifact': str(QA / (code.lower() + '-artifact.xlsx')),
                      'candidate': str(QA / (code.lower() + '-candidate.xlsx')),
                      'edits': list(edits.values())})
    result = {'books': plans, 'issues': issues,
              'preserve': ['81 test IDs/order', 'B:E', 'G existing preparations except explicit condition support', 'rows/merges', 'existing result values and previous pass basis'],
              'basis': 'Current upper matrix U/V; current source; existing recorded unit/system results. No new board execution.'}
    dump('plan.json', result)
    print('Planned:', [(p['code'], len(p['edits'])) for p in plans],
          'condition tests=', sorted({x['test'] for x in common.values()}))


def string_element(c, pkg):
    if c.get('t') == 's':
        return deepcopy(E.fromstring(pkg['xl/sharedStrings.xml'])[int(c.findtext(q('v')))])
    if c.get('t') == 'inlineStr':
        return deepcopy(c.find(q('is')))
    if c.get('t') == 'str' and c.find(q('f')) is None:
        inline = E.Element(q('is'))
        E.SubElement(inline, q('t'), {'{http://www.w3.org/XML/1998/namespace}space': 'preserve'}).text = c.findtext(q('v'), '')
        return inline
    raise AssertionError('Expected a text cell: ' + c.get('r'))


def text_element(element):
    return ''.join(element.xpath('.//s:t/text()', namespaces=NS))


def text_run(inline, value, props=None):
    run = E.SubElement(inline, q('r'))
    if props is not None:
        run.append(deepcopy(props))
    E.SubElement(run, q('t'), {'{http://www.w3.org/XML/1998/namespace}space': 'preserve'}).text = value


def content_runs(c, pkg, edit):
    """Preserve existing rich text for simple replacements and appended content."""
    assert c.find(q('f')) is None, ('Do not replace a formula', edit['sheet'], edit['cell'])
    original = string_element(c, pkg) if c.get('t') in ['s', 'inlineStr', 'str'] else E.Element(q('is'))
    original.tag = q('is')
    if edit['reason'] == '상위 매트릭스의 외부장치 펌웨어 식별 형식':
        for x in original.iter(q('t')):
            if x.text:
                x.text = x.text.replace('SYS-RID-FW-', 'SYS-RID-SP-FW-')
        if text_element(original) != edit['after']:
            # Excel rich-text runs can divide an identifier at any character.
            runs = list(original.iter(q('t')))
            remaining = edit['after']
            for x in runs[:-1]:
                take = len(x.text or '')
                x.text, remaining = remaining[:take], remaining[take:]
            assert runs, ('Missing original text', edit['sheet'], edit['cell'])
            runs[-1].text = remaining
        assert text_element(original) == edit['after'], edit['cell']
        return original
    inline = E.Element(q('is'))
    before, after = edit['before'], edit['after']
    trimmed = before.strip()
    if trimmed and after.lstrip().startswith(trimmed):
        # Preserve the old runs (including blue photo notices) and their boundaries.
        prefix_len = len(before) - len(before.lstrip())
        stop = len(before.rstrip())
        position = 0
        text_run(inline, '\n')
        for x in original:
            if x.tag not in [q('r'), q('t')]:
                continue
            value = ''.join(x.itertext()) if x.tag == q('t') else text_element(x)
            start, end = max(0, prefix_len - position), min(len(value), stop - position)
            if end > start:
                props = x.find(q('rPr')) if x.tag == q('r') else None
                text_run(inline, value[start:end], props)
            position += len(value)
        tail = after[len(after) - len(after.lstrip()) + len(trimmed):]
    else:
        tail = after
    styles = E.fromstring(pkg['xl/styles.xml'])
    xf = styles.find(q('cellXfs'))[int(c.get('s', '0'))]
    base = styles.find(q('fonts'))[int(xf.get('fontId', '0'))]
    base_size = float(base.find(q('sz')).get('val'))
    name = base.find(q('name')).get('val')
    if '[E8300 보완]' in tail or '[QCC 자료 입력]' in tail:
        prefix = '[E8300 보완]' if '[E8300 보완]' in tail else '[QCC 자료 입력]'
        ordinary, notice = tail.split(prefix, 1)
        if ordinary:
            text_run(inline, ordinary)
        props = E.Element(q('rPr'))
        E.SubElement(props, q('rFont'), {'val': name})
        E.SubElement(props, q('sz'), {'val': str(base_size if prefix.startswith('[E8300') else base_size + 2)})
        E.SubElement(props, q('color'), {'rgb': 'FF0070C0' if prefix.startswith('[E8300') else 'FFC00000'})
        if prefix.startswith('[E8300'):
            E.SubElement(props, q('b'))
        else:
            E.SubElement(props, q('u'), {'val': 'single'})
            if base.find(q('b')) is not None:
                E.SubElement(props, q('b'))
        text_run(inline, prefix + notice, props)
    elif tail:
        text_run(inline, tail)
    assert text_element(inline) == after, (edit['cell'], text_element(inline), after)
    return inline


def merge():
    plans = json.loads((QA / 'plan.json').read_text(encoding='utf8'))
    measurements = {}
    if (QA / 'native-check.json').exists():
        measurements = {(x['code'], x['sheet'], x['cell']): x for x in json.loads((QA / 'native-check.json').read_text(encoding='utf-8-sig'))}
    for p in plans['books']:
        pkg = package(Path(p['before']))
        artifact_book = workbook(Path(p['artifact']))
        before_book = workbook(Path(p['before']))
        roots = {}
        for edit in p['edits']:
            assert artifact_book[edit['sheet']]['cells'][edit['cell']] == edit['after']
            part = before_book[edit['sheet']]['part']
            if part not in roots:
                roots[part] = E.fromstring(pkg[part])
            root = roots[part]
            c = cell(root, edit['cell'])
            assert c is not None, ('New cell requires local template', edit['cell'])
            inline = content_runs(c, pkg, edit)
            c.set('t', 'inlineStr')
            for x in list(c):
                c.remove(x)
            c.append(inline)
        # Both main sheets use the same row heights, sized for the wider report scope.
        for part, root in roots.items():
            if part == before_book[p['main']]['part']:
                rows = root.findall('.//s:row', NS)
                for row in rows:
                    r = int(row.get('r'))
                    needs = [x['row_needed'] for x in measurements.values()
                             if x['is_main'] and x['start_row'] <= r <= x['end_row']]
                    if needs:
                        needed = max(needs)
                        if needed > float(row.get('ht', '15')):
                            assert needed <= 409.5, ('Cell needs split inside existing test', r, needed)
                            row.set('ht', str(needed)); row.set('customHeight', '1')
            else:
                for row in root.findall('.//s:row', NS):
                    r = int(row.get('r'))
                    needs = [x['row_needed'] for x in measurements.values()
                             if x['code'] == p['code'] and not x['is_main']
                             and before_book[x['sheet']]['part'] == part
                             and x['start_row'] <= r <= x['end_row']]
                    if needs and max(needs) > float(row.get('ht', '15')):
                        assert max(needs) <= 409.5, ('Height exceeds Excel limit', p['code'], r)
                        row.set('ht', str(max(needs))); row.set('customHeight', '1')
            pkg[part] = E.tostring(root, encoding='UTF-8', xml_declaration=True, standalone=True)
        with ZipFile(p['candidate'], 'w', ZIP_DEFLATED) as z:
            for name, content in pkg.items():
                z.writestr(name, content)
        print(p['code'], 'conservative cell merge complete')


def verify(publish=False):
    baseline = json.loads((QA / 'baseline.json').read_text(encoding='utf8'))
    plan = json.loads((QA / 'plan.json').read_text(encoding='utf8'))
    books = {}
    results = []
    for p in plan['books']:
        a, b = package(Path(p['before'])), package(Path(p['candidate']))
        assert set(a) == set(b)
        sa, sb = workbook(Path(p['before'])), workbook(Path(p['candidate']))
        books[p['code']] = sb
        expected = {(e['sheet'], e['cell']): e for e in p['edits']}
        actual = {(s, addr) for s in sa for addr in set(sa[s]['cells']) | set(sb[s]['cells'])
                  if sa[s]['cells'].get(addr, '') != sb[s]['cells'].get(addr, '')}
        assert actual == set(expected), ('Changed cells differ', p['code'], actual ^ set(expected))
        changed_parts = [n for n in a if a[n] != b[n]]
        allowed_parts = {sa[s]['part'] for s, _ in expected}
        assert set(changed_parts) <= allowed_parts
        for sheet in sa:
            assert sa[sheet]['merges'] == sb[sheet]['merges']
            part = sa[sheet]['part']
            ra, rb = E.fromstring(a[part]), E.fromstring(b[part])
            for (s, addr), e in expected.items():
                if s != sheet:
                    continue
                ca, cb = cell(ra, addr), cell(rb, addr)
                assert sb[s]['cells'][addr] == e['after'] and sa[s]['cells'].get(addr, '') == e['before']
                assert ca.get('s') == cb.get('s')
                cb.getparent().replace(cb, deepcopy(ca))
            for xa, xb in zip(ra.findall('.//s:row', NS), rb.findall('.//s:row', NS)):
                if xa.attrib != xb.attrib:
                    assert float(xb.get('ht')) <= 409.5
                    xb.attrib.clear(); xb.attrib.update(xa.attrib)
            assert E.tostring(ra) == E.tostring(rb), ('Unrelated XML changed', p['code'], sheet)
        main = sb[p['main']]['cells']
        oldmain = sa[p['main']]['cells']
        test_ids = [(addr, value) for addr, value in main.items() if addr.startswith('B') and 'SW-FW-SYS-Test-ID-' in value]
        assert len(test_ids) == 81
        assert test_ids == [(addr, value) for addr, value in oldmain.items() if addr.startswith('B') and 'SW-FW-SYS-Test-ID-' in value]
        sw = {int(v.strip().rsplit('-', 1)[1]) for addr, v in main.items() if addr.startswith('E') and re.fullmatch(r'\s*SW-FW-RID-\d+\s*', v)}
        assert sw == set(range(1, 82))
        upper = {int(x) for v in main.values() for x in re.findall(r'SYS-RID-SP-FW-(\d+)', v)}
        assert upper == set(range(1, 51))
        assert not any('SYS-RID-FW-' in v for sh in sb.values() for v in sh['cells'].values())
        # Existing result values are still in the same cell, apart from a scoped QCC notice correction.
        if p['code'] == 'SSTR':
            for e in p['edits']:
                if e['cell'].startswith('K') and e['cell'] != 'K233':
                    assert e['before'].strip() in e['after']
        results.append({'code': p['code'], 'test_ids': 81, 'software_ids': 81, 'upper_ids': 50,
                        'edited_cells': len(expected), 'changed_parts': changed_parts,
                        'old_results_preserved': True, 'structure_styles_formulas_preserved': True})
    c = books['SSTC']['기준서_시스템 시험']['cells']
    r = books['SSTR']['보고서_시스템 시험']['cells']
    assert all(c.get(addr, '') == r.get(addr, '') for addr in set(c) | set(r)
               if re.fullmatch(r'[A-J]\d+', addr) and 32 <= int(re.search(r'\d+', addr)[0]) <= 281)
    common_heights = []
    for p in plan['books']:
        pkg = package(Path(p['candidate']))
        main_tree = E.fromstring(pkg[books[p['code']][p['main']]['part']])
        common_heights.append({int(x.get('r')): x.get('ht') for x in main_tree.findall('.//s:row', NS)
                               if 32 <= int(x.get('r')) <= 281})
    assert common_heights[0] == common_heights[1], 'Common row heights differ'
    # Another authorized chat may save unrelated documents while this task runs.
    # Preserve those bytes, and require a reviewed, exact hash exception rather
    # than adopting arbitrary workspace changes or replacing them with baseline.
    external_path = QA / 'reviewed-external-changes.json'
    external = json.loads(external_path.read_text(encoding='utf8')) if external_path.exists() else []
    external_hashes = {x['path']: x['current'] for x in external}
    assert set(external_hashes) <= set(baseline['protected'])
    for path, digest in {**baseline['protected'], **baseline['code']}.items():
        assert sha(Path(path)) == external_hashes.get(path, digest), 'Protected file changed: ' + path
    report = {'books': results, 'common_A_J_identical': True, 'common_row_heights_identical': True,
              'unchanged_protected_documents': len(baseline['protected']) - len(external_hashes),
              'external_documents_preserved': external,
              'protected_code_test_files': len(baseline['code']), 'unresolved_scope': plan['issues']}
    if publish:
        native = json.loads((QA / 'native-check.json').read_text(encoding='utf-8-sig'))
        assert all(x['fits'] for x in native)
        assert json.loads((QA / 'visual-review.json').read_text(encoding='utf8'))['issues'] == []
        assert json.loads((QA / 'semantic-review.json').read_text(encoding='utf8'))['issues'] == []
        for p in plan['books']:
            assert sha(Path(p['target'])) == p['sha256'], 'External target edit: ' + p['code']
        for p in plan['books']:
            target = Path(p['target'])
            pending_path = target.with_name(target.name + '.pending')
            assert not pending_path.exists()
            shutil.copyfile(p['candidate'], pending_path)
            pending_path.replace(target)
            assert sha(target) == sha(Path(p['candidate']))
            report[p['code'] + '_sha256'] = sha(target)
    dump('publication.json' if publish else 'checks.json', report)
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    if '--snapshot' in sys.argv:
        snapshot()
    elif '--inspect' in sys.argv:
        inspect()
    elif '--units' in sys.argv:
        units()
    elif '--plan' in sys.argv:
        plan()
    elif '--merge' in sys.argv:
        merge()
    elif '--verify' in sys.argv:
        verify()
    elif '--publish' in sys.argv:
        verify(True)
