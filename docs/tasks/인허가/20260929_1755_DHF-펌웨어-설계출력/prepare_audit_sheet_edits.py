"""Prepare bounded cell edits; existing rows and test identifiers are retained."""
import json
from inspect_audit_units import QA
from patch_audit_designs import PURPOSES

def main():
    data=json.loads((QA/'inspection.json').read_text(encoding='utf8'))
    prior={r['row']:r['cells'] for u in data['suvr']['units'] for r in u['rows']}
    common={}
    result={}
    pad=lambda x:'\n'+x.strip()+'\n'
    def set_(cell,text):common[cell]=pad(text)
    def add(cell,text,out=False):
        col=''.join(c for c in cell if c.isalpha());row=int(''.join(c for c in cell if c.isdigit()))
        (result if out else common)[cell]=pad(prior[row][col].strip()+'\n\n'+text)
    for unit,row in ((1,789),(5,809),(46,1208)):
        set_(f'E{row}',PURPOSES[unit])
    set_('F789','A, T');set_('F809','A, T');set_('F1208','D, A, T')
    add('H789','- 감사기록 파일 없음, 정상 기록 1개, 잘못된 식별자/기록 위치/기록 수를 각각 준비한다. 저장 함수에 실패 응답을 넣는 조건도 분리한다.')
    set_('I789','감사기록 보충 시험은 제품 함수와 RAM 파일로 실행한다. 실제 저장장치의 전원 차단 후 보존은 별도 확인 범위이다.')
    add('J789','- 초기화 후 식별자, 기록 수, 다음 기록 위치와 기존 내용의 보존 여부를 비교한다. 저장 실패 시 반환값도 확인한다.')
    add('K792','조건 [4] 감사기록 준비\n입력: 파일 없음 / 정상 1개 기록 / 식별자 오류 또는 위치=255 또는 기록 수=256\n예상: 없음/오류는 4,096바이트, 식별자 EVENTLOG.TXT, 기록 수=0, 다음 위치=0. 정상 파일은 내용 보존, 다시 쓰기=0회. 모두 반환=0.\n\n조건 [5] 저장 실패\n입력: 새 파일 초기화 중 변경 내용 저장(f_sync) 실패\n예상: 반환=-1(실패).')
    add('L792','검증 [4] PC/RAM 보충 실행\n없음/오류 입력 모두 파일=4,096바이트, 식별자=EVENTLOG.TXT, 기록 수=0, 다음 위치=0, 반환=0이다. 정상 파일은 내용이 같고 다시 쓰기=0회이다. 기준과 일치하여 통과.\n\n검증 [5] PC/RAM 보충 실행\n저장 실패 입력 시 반환=0이다. 기준 -1과 불일치한다.\n[E8300 보완] 저장 실패 반환 처리를 확인한다.',True)
    result['M789']=pad('기존 조건 [1]~[3] 통과\n추가 [4] 통과\n추가 [5] 보완 필요')

    add('H809','- 개발/점검 명령 사용 조건에서 --dump_log를 입력한다. 시간=26-10-02-13-14-15, 주소=12:34:56:78:9A:BC, 이벤트=0x40인 기록 1개를 준비한다.')
    set_('I809','조회 함수의 보충 실행은 제품 함수와 RAM 파일을 사용한다.\n[E8300 확인] RTT 접근 제한과 출하 시 디버그 사용 조건은 추가 확인 대상이다.')
    add('J809','- 조회 명령의 호출 경로를 분석한다. 저장 기록의 표시 문자열과 조회 후 파일 내용이 같은지 비교한다.')
    add('K811','조건 [3] 감사기록 조회\n입력: 준비한 기록 1개, 다음 위치=1\n예상 표시 문자열:\n[LOG] IDENT=EVENTLOG.TXT, WRITE INDEX=1, ENTITY COUNT=1\n      <  0> : TIME {26-10-02-13-14-15} ADDR {12:34:56:78:9A:BC} EVENT {00000040}\n반환=0, 파일 쓰기=0회, 조회 전후 파일 동일.\n읽기 실패 입력 시 반환=-1. 표시 문자열은 색상 제어와 종료 문자를 제외하여 비교한다.')
    add('L811','검증 [3] 코드 분석/PC RAM 보충 실행\n--dump_log에서 감사기록 읽기 함수로 연결된다. 출력 문자열:\n[LOG] IDENT=EVENTLOG.TXT, WRITE INDEX=1, ENTITY COUNT=1\n      <  0> : TIME {26-10-02-13-14-15} ADDR {12:34:56:78:9A:BC} EVENT {00000040}\n반환=0, 파일 쓰기=0회, 파일 내용 동일. 읽기 실패 시 반환=-1이다. 조회 기준과 일치하여 통과.',True)

    add('H1208','- 연결 확인의 시간=26-10-02-13-14-15, 주소=12:34:56:78:9A:BC를 고정한다. 감사기록 파일은 비어 있는 정상 상태로 준비한다.')
    set_('I1208','명령 입력은 E8300 수신 경계에 설정한다. 감사기록/시간 함수의 보충 실행은 RAM 파일과 지정한 시간/tick으로 각각 확인한다.')
    add('J1208','- 연결 확인의 시간 갱신 → 주소 설정 → 이벤트 저장 호출 순서를 분석한다. 보충 실행에서 기록 내용, 시간 증가, 순환 위치와 닫기 실패 반환을 비교한다.')
    add('K1213','시간/주소 반영 예상: 시간 갱신 → 주소 설정 → 연결 이벤트 저장 순서이다. 기록 시간=26-10-02-13-14-15, 주소=12:34:56:78:9A:BC, 이벤트=0x40, 기록 수=1, 다음 위치=1. 기준 시간 갱신 반영 후 1,000 tick(1 ms)을 더하면 초=16이다. 위치254/기록255개에서 저장 후 위치=0, 기록 수=255이다.\n\n조건 [6] 기록 완료 실패\n입력: 파일 닫기(f_close) 실패\n예상: 기록 함수 반환=-1(실패).')
    add('L1213','코드 분석에서 시간 갱신 → 주소 설정 → 연결 이벤트 저장 순서를 확인했다. PC/RAM 저장 함수 실행값은 시간=26-10-02-13-14-15, 주소=12:34:56:78:9A:BC, 이벤트=0x40, 기록 수=1, 다음 위치=1이다. 시간 함수는 1,000 tick 후 초=16이다. 순환 조건은 위치=0, 기록 수=255로 일치하여 통과.\n\n검증 [6] PC/RAM 보충 실행\n닫기 실패 입력 시 반환=0이다. 기준 -1과 불일치한다.\n[E8300 보완] 닫기 실패 반환 처리를 확인한다.',True)
    result['M1208']=pad('기존 조건 [1]~[5] 통과\n추가 정상 조건 통과\n추가 [6] 보완 필요')
    common['C19']=pad('[QCC 자료 입력]')
    common['D19']=pad('QCC 내부 유닛 시험 자료를 여기에 추가한다. 시험 목록은 아래 E8300 시험 다음에 이어서 작성한다.')
    # Same body rows remain aligned in criteria and report.
    payload={'common':common,'report':result,
             'heights':{'19':60,'792':247.5,'811':247.5,'1213':264},
             'qccCells':['C19','D19'],'e8300Cells':['I809','L792','L1213','M789','M1208']}
    (QA/'sheet-edits.json').write_text(json.dumps(payload,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({'common_cells':len(common),'result_cells':len(result),'rows_unchanged':True},ensure_ascii=False))

if __name__=='__main__':main()
