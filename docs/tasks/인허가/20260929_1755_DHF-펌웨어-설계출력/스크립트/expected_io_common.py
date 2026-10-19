"""Build expected-I/O documentation; read-only with respect to all test/product inputs."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[5]
TASK = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/참고/인허가/사운드1/DHF/04_설계 검증/SW/펌웨어/유닛 시험 예상 입출력'
QA = TASK / 'qa/expected-io'

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def snapshot():
    paths = set()
    for name in ('src', 'tests/unit'):
        for p in (ROOT / name).rglob('*'):
            if p.is_file() and (p.suffix.lower() in ('.c','.h','.s','.asm','.ld','.bcf','.py','.json','.ps1') or p.name in ('.cproject','.project')):
                if not any(x in p.parts for x in ('__pycache__','Debug','Release','.metadata')):
                    paths.add(p)
    for p in (ROOT / 'docs/참고/인허가/사운드1').rglob('*'):
        if p.is_file() and p.suffix.lower() in ('.docx','.xlsx') and not p.name.startswith('~$'):
            paths.add(p)
    return {p.relative_to(ROOT).as_posix():digest(p) for p in sorted(paths)}

CASES = {
1: ('빈 파일 초기화', 'RAM 파일이 없고 크기는 0바이트다.', 'ci_event_log_init()을 호출한다.'),
2: ('유효한 기존 기록 유지', '정상 식별자, 기록 수=1, 다음 쓰기 인덱스=1, 0번 이벤트=0x40인 4,096바이트 파일을 준비한다.', 'ci_event_log_init()을 호출한다. 파일 전체가 입력과 같은지 비교한다.'),
3: ('식별자가 잘못된 파일 초기화', '정상 빈 기록에서 식별자 첫 글자만 X로 바꾼다.', 'ci_event_log_init()을 호출한다.'),
4: ('쓰기 인덱스가 잘못된 파일 초기화', '정상 빈 기록에서 다음 쓰기 인덱스를 255로 바꾼다. 유효 인덱스는 0~254다.', 'ci_event_log_init()을 호출한다.'),
5: ('기록 수가 잘못된 파일 초기화', '정상 빈 기록에서 기록 수를 256으로 바꾼다. 최대 저장 수는 255다.', 'ci_event_log_init()을 호출한다.'),
6: ('초기화 중 열기 실패', '파일이 없는 상태에서 파일 열기 함수가 오류 1을 반환하게 한다.', 'ci_event_log_init()을 호출한다.'),
7: ('초기화 중 쓰기 실패', '파일이 없는 상태에서 파일 쓰기 함수가 오류 1을 반환하게 한다.', 'ci_event_log_init()을 호출한다.'),
8: ('이벤트 기록 중 열기 실패', '정상 빈 파일에서 파일 열기 함수가 오류 1을 반환하게 한다.', '연결 이벤트 0x40을 ci_event_log_write()에 전달한다.'),
9: ('이벤트 기록 중 읽기 실패', '정상 빈 파일에서 파일 읽기 함수가 오류 1을 반환하게 한다.', '연결 이벤트 0x40을 기록한다.'),
10: ('이벤트 기록 중 짧은 읽기', '정상 빈 파일에서 오류 코드는 0이지만 요청량보다 1바이트 적게 읽도록 한다.', '연결 이벤트 0x40을 기록한다.'),
11: ('이벤트 기록 중 쓰기 실패', '정상 빈 파일에서 파일 쓰기 함수가 오류 1을 반환하게 한다.', '연결 이벤트 0x40을 기록한다.'),
12: ('이벤트 기록 중 짧은 쓰기', '정상 빈 파일에서 오류 코드는 0이지만 요청량보다 1바이트 적게 쓰도록 한다.', '연결 이벤트 0x40을 기록한다.'),
13: ('식별자가 잘못된 기록 거부', '정상 빈 파일에서 식별자 첫 글자를 X로 바꾼다.', '연결 이벤트 0x40을 기록한다. 초기화와 달리 손상된 파일에 새 이벤트를 쓰지 않아야 한다.'),
14: ('잘못된 쓰기 인덱스 거부', '정상 빈 파일에서 다음 쓰기 인덱스를 255로 바꾼다.', '연결 이벤트 0x40을 기록한다.'),
15: ('잘못된 기록 수 거부', '정상 빈 파일에서 기록 수를 256으로 바꾼다.', '연결 이벤트 0x40을 기록한다.'),
16: ('연결 이벤트 저장', '정상 빈 파일, 시간 바이트 [26,10,2,13,14,15], 시험용 주소 [0x12,0x34,0x56,0x78,0x9A,0xBC]를 준비한다.', '주소 설정 후 연결 이벤트 0x40을 기록한다. 0번 기록의 시간/주소 바이트까지 비교한다.'),
17: ('다음 이벤트 연속 저장', 'C16 직후 상태를 유지한다. 기록 수=1, 쓰기 인덱스=1이다.', '해제 이벤트 0x32를 기록한다. 앞의 연결 기록은 유지하고 1번에 해제 기록을 추가해야 한다.'),
18: ('마지막 슬롯 기록 후 순환', '정상 파일에 기록 수=255, 다음 쓰기 인덱스=254, 0번 이벤트=0x40을 설정한다.', '이벤트 0xF0을 기록한다. 254번에 저장한 뒤 다음 쓰기 인덱스는 0으로 돌아와야 한다.'),
19: ('가장 오래된 슬롯 교체', 'C18 직후 상태를 유지한다. 기록 수=255, 쓰기 인덱스=0이다.', '이벤트 0x32를 기록한다. 0번 기록을 교체하며 기록 수는 255를 유지해야 한다.'),
20: ('주소 해제', '정상 빈 파일과 시험용 주소 [0x12,0x34,0x56,0x78,0x9A,0xBC]를 설정한다.', 'ci_event_log_update_bt_addr(NULL) 호출 뒤 ci_event_log_get_bt_addr()로 읽는다.'),
21: ('정상 기록 읽기', '정상 식별자, 기록 수=0, 쓰기 인덱스=0인 파일을 준비한다.', 'ci_event_log_read()를 호출한다.'),
22: ('기록 읽기 실패', '정상 빈 파일에서 파일 읽기 함수가 오류 1을 반환하게 한다.', 'ci_event_log_read()를 호출한다.'),
23: ('읽기 시 잘못된 인덱스 거부', '정상 빈 파일에서 쓰기 인덱스를 255로 바꾼다.', 'ci_event_log_read()를 호출한다.'),
24: ('초기화 중 저장 동기화 실패', '파일이 없는 상태에서 f_sync()가 오류 1을 반환하게 한다.', 'ci_event_log_init()을 호출한다. 저장 성공을 확정할 수 없으므로 실패를 반환해야 한다는 시험 계약이다.'),
25: ('이벤트 기록 중 닫기 실패', '정상 빈 파일에서 f_close()가 오류 1을 반환하게 한다.', '연결 이벤트 0x40을 기록한다. 저장 완료를 확정할 수 없으므로 실패를 반환해야 한다는 시험 계약이다.'),
}
FIELDS = {
'return':'함수 반환값: 0은 성공, -1은 실패', 'ident':'식별자 12바이트가 EVENTLOG.TXT와 종료문자까지 일치하면 1',
'count':'저장된 기록 수', 'index':'다음에 쓸 슬롯 인덱스', 'file_bytes':'RAM 파일 크기(바이트)',
'close_count':'파일 닫기 호출 횟수', 'no_rewrite':'파일 쓰기 호출 횟수(0이면 기존 파일 유지)',
'preserved':'기존 4,096바이트가 모두 같으면 1', 'no_close_unopened':'열지 못한 파일에 대한 닫기 호출 횟수',
'error_major':'오류 대분류: 10은 데이터 기록 오류', 'error_minor':'오류 세부번호: 1 열기, 2 쓰기, 3 읽기, 4 데이터 형식',
'no_write':'파일 쓰기 호출 횟수', 'event':'0번 슬롯의 이벤트 코드',
'time':'시간 6바이트가 [26,10,2,13,14,15]와 같으면 1',
'address':'주소 6바이트가 [18,52,86,120,154,188]과 같으면 1',
'no_error':'기록된 오류 대분류(0이면 오류 없음)', 'first_event':'0번 슬롯의 이벤트 코드',
'second_event':'1번 슬롯의 이벤트 코드', 'index_wrap':'마지막 슬롯 뒤의 쓰기 인덱스',
'count_saturates':'가득 찬 뒤 유지할 기록 수', 'last_event':'254번 슬롯의 이벤트 코드',
'first_preserved':'0번 슬롯에 남아 있어야 하는 이벤트 코드', 'oldest_replaced':'교체된 0번 슬롯의 이벤트 코드',
'address_zero':'주소 6바이트가 모두 0이면 1',
}

def load_checker():
    path = ROOT / 'tests/unit/check_event_log.py'
    spec = importlib.util.spec_from_file_location('expected_io_event_checker',path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def event_doc():
    module = load_checker()
    lines = ['# 감사기록 시험의 예상 입력과 출력', '',
      '대상: SW-FW-RID-73/74를 보충하는 PC 시험. 유닛 번호를 새로 부여한 시험은 아니다. 25개 조건, 93개 비교값을 정리했다. 아래 값은 실행 전에 정한 기대값이며 실기 결과가 아니다.', '',
      '## 공통 조건', '',
      '- 실제 감사기록 함수 본문을 PC에서 실행하고, 파일/시간/오류 전달만 모의 함수로 바꾼다. 물리 저장소에 쓰지 않는다.',
      '- RAM 파일은 4,096바이트, 기록 하나는 16바이트, 최대 255개다. 식별자는 ASCII `EVENTLOG.TXT`와 종료문자를 포함한 12바이트다.',
      '- 각 조건 시작 시 파일 핸들, 오류/호출 횟수, 주소를 초기화하고 작업 메모리를 0xA5로 채운다. 정상 빈 파일은 식별자 이외의 모든 바이트가 0이다. C17은 C16, C19는 C18의 상태를 이어 쓴다.',
      '- 시험 시간은 `[26,10,2,13,14,15]`로 고정한다. 실제 시계의 정확도나 시간 인증을 검증하는 입력이 아니다.',
      '- 동일 벡터를 최적화 O0/O2로 각각 실행한다. 아래 표는 중복 없이 한 벌만 적었다.', '',
      '실행: 저장소 루트에서 `python tests/unit/check_event_log.py`. 값은 PC 표준 출력의 `RECORD`로 나온다. 이 시험에는 타깃 RTT 자동 출력이나 오실로스코프 파형을 요구하지 않는다.', '']
    records = []
    for n, (title, input_, action) in CASES.items():
        fields = {k.removesuffix('_'):v for k,v in module.EXPECTED[n].items()}
        lines += [f'## C{n}. {title}', '', f'**입력:** {input_}', '', f'**동작:** {action}', '',
                  '| 확인 항목 | 통과 시 예상값 | 의미 |', '|---|---:|---|']
        for field, val in fields.items():
            lines.append(f'| `{field}` | {val} | {FIELDS[field]} |')
        lines += ['', '통과 시 출력되는 비교 기록:', '', '```text']
        for field, val in fields.items():
            lines.append('RECORD '+json.dumps(dict(case=n,field=field,actual=val,expected=val),separators=(',',':')))
        lines += ['```','']
        if n in (24,25):
            lines += ['**현재 불일치:** 기존 PC 시험에서는 `return`이 0으로 나왔다. 사전 기대값 -1과 다르므로 이 조건은 제품 통과로 기록할 수 없다. 위 로그는 문제가 해소되어 기대값과 같을 때의 예다.', '']
        records.append(dict(key=f'C{n}',source='tests/unit/tdc_ut_event_log_host.c',expected_fields=fields,mode='PC/RAM'))
    lines += ['## 전체 판정과 한계', '',
      '- 통과 가정의 프로그램 마지막 줄은 `SUMMARY records=93 mismatches=0`이다. 현재 코드는 C24/C25의 두 차이를 남기므로 실제 기존 결과는 `mismatches=2`다.',
      '- 현재 Python 검사기는 이 두 실패의 위치와 실제값을 유지하는지 검사한다. 출력 `EVENT_LOG_HOST 25 cases, 93 comparisons; 2 known failure records; O0/O2 agree; no product writes/device execution`은 검사기의 정상 실행을 뜻하며 제품 전체 합격 문구가 아니다. 제품 수정 후 두 차이가 없어지면 검사기의 알려진 실패 목록도 별도 검토해야 한다.',
      '- C24는 모의 `f_sync()` 실패를 무시하는 경로를 확인한다. 실제 SDK의 `f_close()`가 내부 동기화를 다시 시도할 수 있으므로 곧바로 실제 데이터 유실로 단정하지 않는다. C25도 실제 NVM 고장 시험은 아니다.',
      '- C17은 해제 이벤트 저장 함수를 직접 부른다. 현재 제품에서 실제 연결 해제 시 그 호출이 이루어지는지는 별도 확인 항목이다.',
      '- `(QCC 검증 결과 필요)` 접근 권한, 인증과 전송 보호는 이 RAM 기록 시험으로 확인되지 않는다. QCC 연계/시간 출처 자료를 받으면 이 절에 입력 조건과 실제 확인 결과를 보완한다.', '',
      '## 근거', '',
      '- 제품 함수: `src/2__cm3/Gen1_5/FS/ci_event_log.c`, 자료형: 같은 폴더의 `ci_event_log.h`.',
      '- 입력: `tests/unit/tdc_ut_event_log_host.c`의 `main`, `begin`, 파일 모의 함수.',
      '- 독립 기대값 목록/완전성 검사: `tests/unit/check_event_log.py`의 `EXPECTED`, `check_records`.', '']
    path = OUT / '공통/감사기록.md'
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text('\n'.join(lines),encoding='utf-8')
    QA.mkdir(parents=True,exist_ok=True)
    (QA / 'common.json').write_text(json.dumps(dict(role='common',auxiliary=[dict(id='EVENT-LOG',file=path.relative_to(ROOT).as_posix(),cases=records)],checks=['25 cases','93 expected fields']),ensure_ascii=False,indent=2),encoding='utf-8')
    assert len(records)==25 and sum(len(c['expected_fields']) for c in records)==93

def main():
    p=argparse.ArgumentParser();p.add_argument('--snapshot',action='store_true');p.add_argument('--verify-preservation',action='store_true')
    args=p.parse_args();QA.mkdir(parents=True,exist_ok=True)
    baseline=QA/'before.json'
    if args.snapshot:
        if baseline.exists(): raise FileExistsError('Do not overwrite initial baseline')
        baseline.write_text(json.dumps(snapshot(),ensure_ascii=False,indent=2),encoding='utf-8')
        print('baseline',len(json.loads(baseline.read_text(encoding='utf-8'))))
    elif args.verify_preservation:
        before=json.loads(baseline.read_text(encoding='utf-8'));after=snapshot()
        diff=[k for k in set(before)|set(after) if before.get(k)!=after.get(k)]
        (QA/'preservation.json').write_text(json.dumps(dict(unchanged=not diff,count=len(before),differences=diff),ensure_ascii=False,indent=2),encoding='utf-8')
        print('preserved',len(before),'differences',len(diff));assert not diff,diff
    else: event_doc();print('event doc: 25 cases / 93 fields')

if __name__=='__main__':main()
