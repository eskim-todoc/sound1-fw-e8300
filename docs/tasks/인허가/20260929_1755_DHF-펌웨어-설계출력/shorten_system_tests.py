"""1.5세대 시험 문체를 참고하여 시스템 시험의 조건과 절차를 간결하게 편집한다."""
import json
import re
import shutil
import sys
from pathlib import Path

import revise_system_readability as previous

TASK = Path(__file__).resolve().parent
QA = TASK / 'qa/system-concise-rev0'
MAIN, CRITERION, HELPER, CYBER = previous.MAIN, previous.CRITERION, previous.HELPER, previous.CYBER
reader = previous.reader


def write_json(name, value):
    (QA / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), 'utf8')


def tests_from(snapshot, sheet_name):
    cells = snapshot['sheets'][sheet_name]['cells']
    starts = sorted([(int(a[1:]), int(re.search(r'(\d+)\s*$', t).group(1)))
                     for a, t in cells.items() if re.fullmatch(r'B\d+', a) and re.fullmatch(r'\s*SW-FW-SYS-Test-ID-\d+\s*', t)])
    return [{'id': ident, 'start': row, 'end': starts[i+1][0]-1 if i+1 < len(starts) else 281,
             'cells': {a: t for a, t in cells.items() if re.fullmatch(r'[C-L]\d+', a)
                       and row <= int(a[1:]) < (starts[i+1][0] if i+1 < len(starts) else 282)}}
            for i, (row, ident) in enumerate(starts)]


def prepare():
    QA.mkdir(parents=True, exist_ok=True)
    assert not (QA / 'baseline.json').exists(), 'Existing input snapshot must not be overwritten.'
    old = json.loads((previous.QA / 'baseline.json').read_text('utf8'))
    baseline = {'files': {}, 'protected': {p: reader.sha(Path(p)) for p in old['protected']}}
    for kind, rec in old['files'].items():
        path = Path(rec['path'])
        baseline['files'][kind] = {'path': str(path), 'sha256': reader.sha(path)}
        shutil.copyfile(path, QA / f'{kind}-before.xlsx')
        write_json(f'{kind}-before.json', reader.extract(path))
    legacy_rec = json.loads((TASK/'qa/system-cybersecurity-rev0/baseline.json').read_text('utf8'))['sources']['legacySystem']
    legacy = Path(legacy_rec['path'])
    baseline['reference'] = {'path': str(legacy), 'sha256': reader.sha(legacy)}
    baseline['protected'][str(legacy)] = reader.sha(legacy)
    write_json('legacy.json', reader.extract(legacy))
    write_json('baseline.json', baseline)
    current = reader.extract(QA/'system-before.xlsx')
    tests = tests_from(current, MAIN)
    assert len(tests) == 81
    write_json('tests.json', tests)
    for lo, hi in [(1,26),(27,54),(55,81)]:
        write_json(f'tests-{lo}-{hi}.json', [x for x in tests if lo <= x['id'] <= hi])
    print(json.dumps({'tests':len(tests),'legacySheets':list(reader.extract(legacy)['sheets']),'protected':len(baseline['protected'])},ensure_ascii=False))


def parent_proposals():
    source = reader.extract(QA/'system-before.xlsx')['sheets'][MAIN]['cells']
    fixture = ('외부기와 내부기를 연결한다.\n내부기를 출력 측정용 지그에 연결하고 전극을 PBS 용액에 담근다.\n'
               '오실로스코프를 지그에 연결한다.\n또는 32채널 핀 헤더 내부기 테스트 지그를 사용한다.')
    data = {}
    def put(cell, text):
        data[cell] = text.strip()
    def base(row, desc, setup=None, assumption=None, procedure=None):
        put(f'C{row}', desc)
        if setup is not None: put(f'G{row}', setup)
        if assumption is not None: put(f'H{row}', assumption)
        if procedure is not None: put(f'I{row}', procedure)
    base(34, '최대 자극률을 확인한다.', fixture,
         '활성화된 자극 채널수: 24개 이하\n펄스폭: 13 us',
         '오실로스코프로 내부기에서 출력되는 펄스파형의 전체 자극률을 측정한다.')
    put('J34', '조건 [1]\n최대 자극률: 24 kHz, 허용오차 ±5%\n허용 범위: 22.8~25.2 kHz')
    base(35, '채널별 최대 자극률을 확인한다.', fixture,
         '활성화된 자극 채널수: 24개 이하\n펄스폭: 13 us\n자극 채널을 구분할 수 있는 맵이 적용되어야 한다.',
         '오실로스코프로 각 활성 채널의 펄스파형과 자극률을 측정한다.')
    for n, row in enumerate(range(35,38), 1):
        channels = ', '.join(map(str,range((n-1)*8+1,n*8+1)))
        put(f'J{row}', f'조건 [{n}] 측정 채널: {channels}\n채널별 자극률: 1,000 Hz, 허용오차 ±5%\n모든 측정 채널이 950~1,050 Hz이어야 한다.')
    base(38, '모노폴라 방식의 자극 출력을 확인한다.', fixture,
         '모노폴라 자극 출력이 설정된 맵이 적용되어야 한다.',
         '오실로스코프로 자극 전극과 기준전극 사이의 펄스파형을 확인한다.')
    put('J38', '조건 [1] 모노폴라 모드 2, 자극 전극 1, 기준전극 99\n펄스폭 25 us, 전류 300 uA, 음극 선행\n전극 1과 기준전극 사이에 자극 출력이 확인되어야 한다.')
    base(39, '맵에 설정한 방향으로 첫 펄스가 출력되는지 확인한다.', fixture,
         '음극 선행과 양극 선행을 각각 확인할 맵이 준비되어야 한다.',
         '각 맵을 적용하고 오실로스코프로 첫 펄스의 방향을 확인한다.')
    for row,n,value,phase in [(39,1,0,'음극'),(40,2,1,'양극')]:
        put(f'J{row}', f'조건 [{n}] 선행 위상 설정(firstPulsePhase)={value}\n펄스폭 25 us, 전류 300 uA\n첫 펄스가 {phase} 방향으로 출력되어야 한다.')
    base(41, '설정한 전류에 맞게 자극이 출력되는지 확인한다.', fixture,
         '시험할 전류가 설정된 맵이 적용되어야 한다.\n지그의 전류 측정 저항값이 확인되어야 한다.',
         '오실로스코프로 자극 펄스의 전압을 측정한다.\n지그의 측정 저항값으로 전류를 환산하여 설정값과 비교한다.')
    for row,n,value,lo,hi in [(41,1,500,450,550),(42,2,1000,900,1100),(43,3,1500,1350,1650)]:
        put(f'J{row}', f'조건 [{n}] 설정 전류 {value:,} uA, 펄스폭 25 us\n허용오차 ±10%\n측정 전류가 {lo:,}~{hi:,} uA이어야 한다.')
    base(44, '설정한 펄스폭으로 자극이 출력되는지 확인한다.', fixture,
         '시험할 펄스폭이 설정된 맵이 적용되어야 한다.',
         '오실로스코프로 음극과 양극 펄스의 폭을 각각 측정한다.')
    for row,n,value,lo,hi in [(44,1,13,12.35,13.65),(45,2,25,23.75,26.25),(46,3,100,95,105),(47,4,255,242.25,267.75)]:
        put(f'J{row}', f'조건 [{n}] 설정 펄스폭 {value} us, 허용오차 ±5%\n음극과 양극 펄스의 폭이 각각 {lo}~{hi} us이어야 한다.')
    base(48, '최대 전하량을 초과하면 오류가 표시되는지 확인한다.',
         '외부기를 내부기에 연결한다.', '최대 전하량을 초과하도록 설정한 시험 맵이 준비되어야 한다.',
         '시험 맵을 적용하고 오류 LED를 확인한다.')
    put('J48','조건 [1] T=100 uA, C=300 uA, 펄스폭 251 us, 32채널\n전하량 75,300 pC가 한도 75,000 pC를 초과하므로 붉은색 오류 LED가 표시되어야 한다.')
    put('J49','조건 [2] 계산 보조 확인: T=100 uA, C=2500 uA, 펄스폭 25 us\n출력 가능한 전류 범위를 초과하므로 계산 실패가 반환되어야 한다.')
    receiver='음원 재생 장치와 OD1 출력 수신기를 준비한다.\n수신기는 차동 PDM 입력을 지원하거나 아날로그 변환 회로를 갖춰야 한다.'
    base(50, '오디오 볼륨에 따른 AGC 출력 소리의 변화를 확인한다.', receiver,
         '같은 음원, 입력 레벨, 맵과 자극 볼륨을 사용해야 한다.',
         '오디오 볼륨을 1, 5, 10으로 바꾸며 OD1 출력 소리를 듣는다.')
    for n,(row,vol) in enumerate([(50,1),(51,5),(52,10)],1):
        put(f'J{row}',f'조건 [{n}] 오디오 볼륨 {vol}\n조회한 볼륨이 {vol}이어야 한다.\n볼륨 변경에 따른 출력 소리 크기의 차이가 들려야 한다.')
    base(53,'FFT 방식으로 소리의 주파수를 분석하는지 확인한다.',
         assumption='', procedure='소스코드에서 FFT 처리 순서를 확인한다.')
    put('J53','조건 [1]\n512표본에 창 함수를 적용하여 실수 FFT를 수행하고, 256개 크기 값을 계산해야 한다.\n완료 이벤트 후 CFX에서 대역을 분석해야 한다.')
    base(54,'입력 주파수에 해당하는 채널에서 자극이 나오는지 확인한다.', fixture+'\n순음 발생기를 준비한다.',
         '32개 분석 대역이 활성화된 맵이 적용되어야 한다.',
         '각 분석 대역의 순음을 하나씩 입력하고, 해당 채널의 자극 출력을 오실로스코프로 확인한다.')
    for n,row in enumerate([54,56,57,58,59,60,61,62],1):
        bands=', '.join(map(str,range((n-1)*4+1,n*4+1)))
        put(f'J{row}',f'조건 [{n}] 분석 대역: {bands}\n각 대역에 해당하는 순음을 하나씩 입력한다.\n입력한 대역에 배정된 전극에서 자극 출력이 확인되어야 한다.')
    put('J55','1~32개 분석 대역 설정을 지원해야 한다.')
    put('J63','조건 [9] 계산 보조 확인: CIS 1/11밴드, 채널당 2프레임, 블록당 12채널\n설정한 분석 대역에 대응하는 채널의 출력이 확인되어야 한다.')
    base(64,'CIS와 NofM의 채널 선택 및 자극 순서를 확인한다.',fixture,
         'CIS 시험 맵과 기본 맵 3번(NofM)이 준비되어야 한다.\nNofM은 32개 대역 중 16개를 선택하도록 설정되어야 한다.',
         '1. CIS 맵을 적용하고 자극 순서를 확인한다.\n2. 맵 3번에서 16개보다 많은 대역의 소리를 입력한다.\n3. 선택된 16채널의 자극 유무와 순서를 오실로스코프로 확인한다.')
    put('J64','조건 [1] CIS, 대역과 전극을 1:1로 설정\n자극 순서가 맵 설정과 같아야 한다.\n예상 순서=[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32]')
    put('J65','조건 [2] 맵 3번, N=16/M=32의 선택 계산 보조 확인\n대역 인덱스 0~31의 자극 크기 코드=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31]\n전극 번호=대역 인덱스+1\n선택 대역=[16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31]\n출력 채널=[17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32]\n무자극 채널=[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]')
    put('J66','조건 [3] 맵 3번, 출력 순서 보조 확인\n선택 채널=[17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32], 직전 마지막 대역 인덱스=31\n전체 순서=[17,25,18,26,19,27,20,28,21,29,22,30,23,31,24,32]\n첫 1 ms: [17,25,18,26,19,27,20,28]\n다음 1 ms: [21,29,22,30,23,31,24,32]\n16채널이 위 순서로 2 ms 동안 출력되어야 한다.')
    put('J67','조건 [4] 계산 보조 확인: CIS 1/11밴드, 채널당 2프레임, 블록당 12채널\n설정한 순서대로 자극 데이터가 유지되어야 한다.\n필요한 자극 데이터가 NOP(무자극)로 바뀌거나 누락되지 않아야 한다.')
    base(68,'오디오 처리 주기가 1 ms인지 확인한다.', '오디오 처리 시작 시각을 저장하고 RTT로 읽을 수 있게 준비한다.',
         '정상 오디오 처리 중에 시험해야 한다.',
         '연속한 처리 시작 시각을 저장한 뒤 RTT로 읽는다.\n시작 시각의 차이로 처리 주기를 확인한다.')
    put('J68','조건 [1] 연속한 처리 시작 시각 5개(t[0]~t[4])를 확인한다.\n각 시작 간격이 1.00 ms, 허용오차 ±5% 이내이어야 한다.\n허용 범위: 0.95~1.05 ms')
    put('J69','조건 [2]\nRTT로 읽은 시작 시각이 저장된 시각과 같아야 한다.')
    base(70,'맵 선택에 따라 자극 출력이 바뀌는지 확인한다.',fixture+'\n리모콘을 연결한다.',
         '출력을 구분할 수 있는 맵 4개가 저장되어야 한다.',
         '리모콘으로 맵 1, 2, 3, 4를 선택하며 오실로스코프로 자극 출력을 확인한다.')
    for n,row in enumerate(range(70,74),1): put(f'J{row}',f'조건 [{n}] 맵 {n} 선택\n선택 맵 번호가 {n}이고, 해당 맵의 자극 출력이 확인되어야 한다.')
    base(74,'자극 볼륨 4단계의 출력 크기 변화를 확인한다.',fixture+'\n리모콘을 연결한다.',
         '같은 맵과 입력 음원을 사용해야 한다.',
         '자극 볼륨을 1~4단계로 바꾸며 오실로스코프로 출력 크기를 확인한다.')
    for n,(row,code) in enumerate([(74,119),(75,129),(76,139),(77,150)],1):
        vector=re.search(r'Cnew 코드\[0~31\]=(\[[^\]]+\])',source[f'J{row}']).group(1)
        put(f'J{row}',f'조건 [{n}] 자극 볼륨 {n}\n볼륨 단계에 따라 출력 크기가 달라져야 한다.\n계산 보조 입력: 32채널, T 코드=50, C 코드=150\n예상 Cnew 코드[0~31]={vector}')
    base(78,'[E8300/QCC 각각 확인] 두 마이크와 I2S 오디오 입력을 처리할 수 있는지 확인한다.',
         '마이크 2개와 QCC의 I2S 오디오 입력을 준비한다.', assumption='',
         procedure='각 마이크와 I2S에 차례로 소리를 입력한다.\n입력 버퍼의 변화와 I2S에 따른 자극 출력을 확인한다.')
    put('J78','조건 [1] 마이크1에만 소리 입력\n마이크1 입력 버퍼에 변화가 확인되어야 한다.')
    put('J79','조건 [2] 마이크2에만 같은 소리 입력\n마이크2 입력 버퍼에 변화가 확인되어야 한다.\n마이크1과 마이크2를 각각 처리할 수 있어야 한다.')
    put('J80','조건 [3] 외부 음원이 없는 상태에서 QCC I2S를 OFF→ON→OFF로 변경한다.\nI2S 입력 버퍼의 변화와 ON 구간의 자극 출력 변화가 확인되어야 한다.')
    put('J81','조건 [4] 복사 완료 플래그 보조 확인\nI2S 복사 직후 copy_done=1(완료), 해제 후 copy_done=0이어야 한다.')
    base(82,'100 Hz와 8000 Hz 입력 소리를 처리할 수 있는지 확인한다.',
         '순음 발생기를 준비한다.\n'+receiver, assumption='',
         procedure='100 Hz와 8000 Hz 순음을 각각 입력하고 OD1 수신기에서 출력 소리를 듣는다.')
    for row,n,f in [(82,1,100),(83,2,8000)]: put(f'J{row}',f'조건 [{n}] {f} Hz 순음 입력\nOD1 수신기에서 입력에 따른 소리가 들려야 한다.')
    base(84,'자극 데이터가 FPGA에 전달되고 정상 상태가 표시되는지 확인한다.',fixture,
         'FPGA와 내부기의 초기화가 완료되어야 한다.\nLED 표시를 허용하고 오류, 매핑 및 업데이트가 없는 상태여야 한다.',
         '소리를 입력하고 FPGA의 정상 응답, 내부기 자극 출력 및 LED를 확인한다.')
    put('J84','조건 [1] 내부기 연결 후 소리 입력\nFPGA의 정상 응답과 내부기의 자극 출력이 확인되어야 한다.')
    put('J85','조건 [2] 정상 APP, 내부기 연결, LED 표시 허용\n백색 LED가 계속 켜져 있어야 한다.')
    base(86,'자극 기능 시험 결과를 종합 확인한다.', assumption='시스템 시험 ID 1~7이 완료되어야 한다.',
         procedure='시스템 시험 ID 1~7의 측정 결과와 판정을 확인한다.')
    for n,row in enumerate(range(86,93),1): put(f'J{row}',f'조건 [{n}] SW-FW-SYS-Test-ID-{n}\n해당 시험 결과가 통과이어야 한다.')
    base(93,'터치 입력과 그에 따른 전원 동작을 확인한다.',
         assumption='부팅과 터치 센서 준비가 완료되어야 한다.',
         procedure='터치 센서를 누르고 손을 떼며 입력 상태를 확인한다.\n길게 누를 때 절전 진입과 복귀 동작을 확인한다.')
    put('J93','조건 [1] 센서 준비 상태 READY에서 누르기/유지/손 떼기\n입력이 NOT_TOUCH→TOUCH→NOT_TOUCH로 바뀌고 사용자 제어에 반영되어야 한다.')
    put('J94','조건 [2] 내부기/매핑 미연결, 절전 허용 상태에서 길게 누르기\n2399/2400/2401 ms의 길게 누름 이벤트=[false,true,false]\n2400 ms에 한 번 발생하고, 전원 꺼짐 표시와 자극 해제 후 절전 모드로 전환되어야 한다.')
    put('J95','조건 [3] 절전 상태에서 손을 뗀 뒤 다시 길게 누르기\n다시 기동하여 정상 모드로 복귀해야 한다.')
    base(96,'전원 켜짐과 내부기 연결 상태의 LED 표시를 확인한다.',
         assumption='LED 표시가 허용되고 오류, 매핑 및 업데이트가 없는 상태여야 한다.',
         procedure='전원을 켠 뒤 대기 상태의 LED를 확인한다.\n내부기를 연결하고 LED를 확인한다.')
    put('J96','조건 [1] 정상 APP, 배터리 잔량 80% 이상, 내부기 미연결\n하늘색 LED가 4회 깜박인 뒤 녹색 LED가 계속 켜져 있어야 한다.')
    put('J97','조건 [2] 내부기 연결\n백색 LED가 계속 켜져 있어야 한다.')
    led_setup='충전기와 외부기를 준비한다.\n크래들 뚜껑을 열어 LED를 확인할 수 있게 한다.'
    led_assumption='LED 표시가 허용되어야 한다.\n내부기/매핑 미연결이며 오류, 업데이트 및 페어링 표시가 없는 상태여야 한다.'
    for start,desc in [(98,'충전 여부에 따른 배터리 LED 표시를 확인한다.'),(106,'배터리 잔량에 따른 LED 표시를 확인한다.')]:
        base(start,desc,led_setup,led_assumption,
             '충전기를 연결하거나 분리한 상태에서 배터리 잔량과 LED 색상/점멸을 확인한다.')
        for n,(state,batt,color,pattern) in enumerate([(s,b,c,p) for s in ['분리','연결'] for b,c,p in [(80,'녹색','계속 켜져'),(79,'노란색','계속 켜져'),(10,'노란색','계속 켜져'),(9,'노란색','깜박이고')]],1):
            ending=f'{color} LED가 {pattern} 있어야 한다.' if batt!=9 else '노란색 LED가 깜박여야 한다.'
            put(f'J{start+n-1}',f'조건 [{n}] 충전기 {state}, 배터리 잔량 {batt}%\n{ending}')
    base(114,'배터리가 부족하면 알림용 자극이 출력되는지 확인한다.',fixture+'\n외부기를 전원공급기에 연결한다.',
         '내부기가 연결되고 배터리 부족 자극 알림이 켜져 있어야 한다.',
         '전원을 3.7 V에서 3.3 V로 낮춘다.\n배터리 잔량과 알림용 자극 파형을 확인한다.')
    put('J114','조건 [1] 자극 알림 사용, 전원 3.7 V→3.3 V\n배터리 부족 상태에서 알림용 자극이 출력되어야 한다.')
    put('J115','조건 [2] 잔량 경계 확인: 내부기 연결, 알림 허용\n배터리 잔량 20%에서는 부족 알림을 요청하지 않아야 한다.\n19%에서는 알림 발생 조건이 되면 알림용 자극이 출력되어야 한다.')
    base(116,'리모콘의 맵 변경에 따라 자극 출력이 바뀌는지 확인한다.',fixture+'\n리모콘을 연결한다.',
         '서로 다른 출력의 맵 4개를 사용할 수 있어야 한다.',
         '리모콘에서 맵을 변경하고 오실로스코프로 자극 출력을 확인한다.')
    for n,(row,old,new) in enumerate([(116,1,2),(117,2,3),(118,3,4),(119,4,1)],1):
        put(f'J{row}',f'조건 [{n}] 사용 가능 맵=[1,1,1,1], 현재 맵 {old}\n맵 증가 명령 [0x44,1] 입력\n선택 맵={new}, 응답=[0x44,{new}]\n선택한 맵에 맞게 자극 출력이 바뀌어야 한다.')
    put('J120','조건 [5] 명령 0x4A 처리 중 새 명령 입력\n처리 중 오류 응답 0xF0/1을 보내고 이전 명령 0x4A를 유지해야 한다.')
    base(121,'리모콘의 오디오 볼륨 변경에 따라 자극 크기가 바뀌는지 확인한다.',fixture+'\n리모콘을 연결한다.',
         '같은 음원, 맵과 자극 볼륨을 사용해야 한다.',
         '리모콘에서 오디오 볼륨을 바꾸고 오실로스코프로 자극 크기를 확인한다.')
    for n,(row,old,cmd,new) in enumerate([(121,1,1,2),(122,5,1,6),(123,10,2,9)],1):
        put(f'J{row}',f'조건 [{n}] 오디오 볼륨 {old}에서 [0x46,{cmd}] 입력\n볼륨={new}, 응답=[0x46,{new}]\n오디오 볼륨 변경에 따른 자극 크기의 변화가 확인되어야 한다.')
    base(124,'리모콘의 자극 볼륨 변경에 따라 출력 크기가 바뀌는지 확인한다.',fixture+'\n리모콘을 연결한다.',
         '같은 음원, 맵과 오디오 볼륨을 사용해야 한다.',
         '리모콘에서 자극 볼륨을 바꾸고 오실로스코프로 출력 크기를 확인한다.')
    for n,(row,old,cmd,new) in enumerate([(124,1,1,2),(125,2,1,3),(126,3,1,4),(127,4,2,3)],1):
        put(f'J{row}',f'조건 [{n}] 자극 볼륨 {old}에서 [0x45,{cmd}] 입력\n볼륨={new}, 응답=[0x45,{new}]\n자극 볼륨 변경에 따른 출력 크기의 변화가 확인되어야 한다.')
    write_json('proposals-1-26.json',{'reviewedIds':list(range(1,27)),
        'changes':[{'cell':a,'after':t,'reason':'1.5세대 문체에 맞춰 시험 전제/동작/판정을 간결하게 정리'} for a,t in data.items()],
        'issues':[{'id':1,'note':'사용자 지정으로 32채널/20kHz 이상 기준을 24채널 이하/13us/24kHz±5%로 수정. 기존 결과는 20kHz 이상으로만 기록되어 새 수치를 측정 결과로 만들어 쓰지 않는다.'}]})
    print('Parent proposals:',len(data))


def plan():
    snapshot = reader.extract(QA/'system-before.xlsx')
    original = snapshot['sheets'][MAIN]['cells']
    changes = {}
    def put(sheet, cell, text, reason):
        clean = text.strip().replace('·', '/')
        value = '\n'+clean+'\n' if clean else ''
        if snapshot['sheets'][sheet]['cells'].get(cell,'') != value:
            changes[(sheet,cell)] = {'sheet':sheet,'cell':cell,'after':value,'reason':reason}
    reviewed=[]; issues=[]
    for lo,hi in [(1,26),(27,54),(55,81)]:
        proposal=json.loads((QA/f'proposals-{lo}-{hi}.json').read_text('utf8'))
        reviewed.extend(proposal['reviewedIds']); issues.extend(proposal.get('issues',[]))
        owned=[x for x in json.loads((QA/'tests.json').read_text('utf8')) if lo<=x['id']<=hi]
        allowed_rows={r for t in owned for r in range(t['start'],t['end']+1)}
        for item in proposal['changes']:
            assert re.fullmatch(r'[CGHIJ]\d+',item['cell']) and int(item['cell'][1:]) in allowed_rows
            put(MAIN,item['cell'],item['after'],item['reason'])
    assert sorted(reviewed)==list(range(1,82))
    # 최종 편집: 시험 동작과 판정을 짧게 쓰고, 계측 세부 기호는 반복하지 않는다.
    refinements = {
        'I130':'리모콘에서 자극 알림을 끈다.\n전원을 3.7 V에서 낮추며 배터리 부족 LED와 알림용 자극의 유무를 확인한다.',
        'H130':'같은 내부기, 맵과 음원을 사용해야 한다.\n알림을 켰을 때의 반복 간격이 확인되어야 한다.',
        'I146':'리모콘에서 자극 알림을 끄고 앱 표시를 확인한다.\n전원을 3.7 V에서 낮추며 배터리 부족 LED와 알림용 자극의 유무를 확인한다.',
        'H146':'같은 내부기, 맵과 음원을 사용해야 한다.\n알림을 켰을 때의 반복 간격이 확인되어야 한다.',
        'I162':'외부기를 켜고 FPGA I2C 통신 상태를 확인한다.\nLED와 오실로스코프의 자극 출력을 확인한다.',
        'I166':'외부기를 켜고 PMIC I2C 설정 결과를 확인한다.\nLED와 오실로스코프의 자극 출력을 확인한다.',
        'I188':'내부기 미연결, 등록 내부기 연결, 미등록 내부기 연결 상태에서 각각 광고를 검색한다.\n각 상태의 자극 출력과 LED를 확인한다.',
        'H190':'QCC의 무조작 연결 종료 시간이 확인되어야 한다.\n상대 장치의 자동 상태 조회가 중지되어야 한다.',
        'I190':'페어링과 인증 후 제어 명령을 보낸다.\n조작을 멈추고 30분 동안 연결 유지 및 종료 시각을 확인한다.',
        'I237':'전원을 3.7 V에서 3.3 V로 낮춘다.\n오실로스코프로 배터리 부족 알림용 자극과 연속 3회 알림의 간격을 측정한다.',
        'H244':'오디오 처리를 멈추지 않고 같은 타이머로 처리 시작/완료 시각을 저장할 수 있어야 한다.\n타이머의 시간 단위와 기록 누락 여부를 확인할 수 있어야 한다.',
        'I244':'정상 실시간 자극 중 각 오디오 블록의 처리 시작/완료 시각을 저장한다.\n측정 후 RTT로 읽어 처리 주기와 다음 블록 시작 전 완료 여부를 확인한다.',
        'J244':'조건 [1] 정상 실시간 자극 중 처리 시작 간격 측정\n처리 주기: 1.00 ms, 허용오차 ±5%\n모든 측정 주기가 0.95~1.05 ms이고 다음 블록 시작 전에 처리가 완료되어야 한다.',
        'J245':'조건 [2] 블록별 처리 완료 시각과 다음 시작 시각 비교\n모든 블록이 다음 블록의 시작 전에 처리를 마쳐야 한다.',
        'I278':'외부기의 시동과 내부기 부착 과정에서 오실로스코프로 자극 파형을 확인한다.\n시동 완료 시각과 비교한다.',
        'I280':'외부기의 종료와 내부기 탈착 과정에서 오실로스코프로 자극 파형을 확인한다.\n종료 완료 시각과 비교한다.',
    }
    for cell,text in refinements.items(): put(MAIN,cell,text,'실행 동작과 필요한 전제만 간단히 정리')
    put(MAIN,'J41',changes[(MAIN,'J41')]['after'].strip()+'\n자극칩 규격의 최소/중간/최대 설정에서도 같은 오차 기준을 만족해야 한다.','전류 예시 외의 원래 시험 범위 유지')
    # 기존 20 kHz 이상 기록만으로 새 24 kHz ±5% 기준의 측정값을 확정할 수 없다.
    # 기존 확인 결과를 보존하고 변경된 기준의 사진 자리를 구분한다.
    put(MAIN,'K34','검증 [1]\n기존 기록: 전체 자극률 20,000회/s(20 kHz) 이상 확인.\n\n(사진필요) 24 kHz ±5% 기준의 최대 자극률 측정 파형.','수정된 시험 조건의 사진 자리와 기존 기록 구분')
    put(MAIN,'L34','기존 기록 통과\n24 kHz 기준 사진 확인 대기','새 수치 결과를 만들지 않고 첨부 확인 상태 표시')
    put('예상 그림','C6','(사진필요) 24 kHz ±5% 기준의 최대 자극률 측정 파형.\n활성 채널 24개 이하, 펄스폭 13 us.','수정된 조건에 맞게 사진 자리 안내')
    # 사진 안내에는 촬영 대상을 적고 부수적인 계산표/원문 저장 요구는 반복하지 않는다.
    helper=snapshot['sheets'][HELPER]['cells']
    main_conditions={}
    for test in json.loads((QA/'tests.json').read_text('utf8')):
        for row in range(test['start'],test['end']+1):
            cell=f'J{row}'; text=changes.get((MAIN,cell),{}).get('after',original.get(cell,''))
            match=re.search(r'조건\s*\[(\d+)\]',text)
            if match: main_conditions[(test['id'],int(match.group(1)))]=text
    helper_id=None
    for row in range(1,500):
        ident=re.search(r'SW-FW-SYS-Test-ID-(\d+)',helper.get(f'A{row}',''))
        if ident: helper_id=int(ident.group(1))
        old=helper.get(f'B{row}',''); condition=re.search(r'조건\s*\[(\d+)\]',old)
        if condition and (helper_id,int(condition.group(1))) in main_conditions:
            put(HELPER,f'B{row}',main_conditions[(helper_id,int(condition.group(1)))],'본문과 동일한 조건/판정')
        if helper_id==1 and condition:
            put(HELPER,f'C{row}','기존 기록: 전체 자극률 20 kHz 이상 확인.\n\n(사진필요) 24 kHz ±5% 기준의 최대 자극률 측정 파형.','최대 자극률 사진 안내 간결화')
            put(HELPER,f'D{row}','기존 기록 통과\n24 kHz 기준 사진 확인 대기','본문 판정과 맞춤')
    ordered=sorted(changes.values(),key=lambda x:(list(snapshot['sheets']).index(x['sheet']),int(x['cell'][1:]),x['cell'][0]))
    assert not any('·' in x['after'] for x in ordered)
    # 높이 검사는 결과 열까지 포함한다. 짧아진 절차에 맞추어 행을 줄이되 결과 글씨는 잘리지 않아야 한다.
    measure={(x['sheet'],x['cell']):x for x in ordered}
    for a,t in original.items():
        if re.fullmatch(r'[B-L]\d+',a) and 34<=int(a[1:])<=281 and t.strip():
            measure.setdefault((MAIN,a),{'sheet':MAIN,'cell':a,'after':t})
    old_heights=json.loads((QA/'plan.json').read_text('utf8')).get('heights',[]) if (QA/'plan.json').exists() else []
    write_json('plan.json',{'changes':ordered,'measure':list(measure.values()),'heights':old_heights,'reviewedIds':reviewed,'issues':issues})
    print(json.dumps({'changes':len(ordered),'main':sum(x['sheet']==MAIN for x in ordered),'blankAssumptions':sum(not changes.get((MAIN,f'H{x["start"]}'),{}).get('after',original.get(f'H{x["start"]}','')).strip() for x in json.loads((QA/'tests.json').read_text('utf8')))},ensure_ascii=False))


def build():
    data=json.loads((QA/'plan.json').read_text('utf8'))
    authored=reader.extract(QA/'system-authored.xlsx')
    for kind in ['system','criterion']:
        package=previous.native.parts(QA/f'{kind}-before.xlsx')
        edits=previous.expected_changes(kind,data)
        names={x['sheet'] for x in edits}|{x['sheet'] for x in data['heights'] if kind=='system' or x['sheet'] in [MAIN,CYBER]}
        for source_name in names:
            name=CRITERION if kind=='criterion' and source_name==MAIN else source_name
            part=previous.native.sheet_path(package,name); root=previous.native.parse(package[part])
            cells={x.get('r'):x for x in root.findall('.//s:c',previous.native.N)}
            for item in [x for x in edits if x['sheet']==source_name]:
                assert authored['sheets'][source_name]['cells'].get(item['cell'],'')==item['after']
                previous.bridge.inline(cells[item['cell']],item['after'])
            for item in [x for x in data['heights'] if x['sheet']==source_name]:
                row=root.find(f's:sheetData/s:row[@r="{item["row"]}"]',previous.native.N)
                assert item['height']<=409.5
                row.set('ht',str(item['height'])); row.set('customHeight','1')
            package[part]=previous.native.serialize(root)
        if kind=='system':
            package['xl/media/aligned_system_image.png']=(QA/'SYS-01-rate-24k.png').read_bytes()
        previous.bridge.save(package,QA/f'{kind}-candidate.xlsx')
    print('Original packages updated with Artifact-authored cell values.')


def draw_rate():
    """기존 SVG 도식 생성기를 이용해 새 기준의 수치 파형을 다시 그린다."""
    import draw_validation_expected as drawing
    drawing.OUT=QA
    f=drawing.Figure('SYS-01-rate-24k','최대 자극률: 24 kHz',[
        '허용 범위: 22.8~25.2 kHz (24 kHz, 허용오차 ±5%).',
        '활성 채널 24개 이하, 펄스폭 13 us. 높이는 설명용 상대값이다.'
    ],drawing.sysids(1))
    f.text(36,146,'출력 전류 (상대값)',26)
    f.line([(145,374),(1060,374)],'#BBC2C8',2)
    # 가로 230픽셀은 41.6667 us, 한 위상 71.76픽셀은 13 us이다.
    for x,label in zip([170,400,630,860],['0','41.67','83.33','125']):
        f.line([(x-15,374),(x,374),(x,246),(x+71.76,246),(x+71.76,374),
                (x+82,374),(x+82,486),(x+153.76,486),(x+153.76,374)],drawing.BLUE,4)
        f.text(x-15,523,label,24,drawing.GRAY)
    f.bracket(170,400,210,'41.67 us')
    f.text(879,553,'시간 (us)',23,drawing.GRAY)
    f.save('24 kHz에서 연속 자극 시작 간격의 예시.','사용자 지정 시스템 시험 ID1 기준')


def audit_content():
    """문구 축약과 별도로 입력/응답 배열 및 숫자 변경을 검토한 근거를 남긴다."""
    previous.QA=QA
    previous.audit()
    data=json.loads((QA/'plan.json').read_text('utf8'))
    source=reader.extract(QA/'system-before.xlsx')['sheets'][MAIN]['cells']
    edits=[x for x in data['changes'] if x['sheet']==MAIN and re.fullmatch(r'[CGHIJ]\d+',x['cell'])]
    before=sum(len(source.get(x['cell'],'')) for x in edits)
    after=sum(len(x['after']) for x in edits)
    summary={
        'reviewedTests':81,'editedMainCommonCells':len(edits),'beforeChars':before,'afterChars':after,
        'reductionPercent':round(100*(1-after/before),1),
        'numericReviewNotes':[
            '시험1의 32채널/20 kHz 이상은 사용자 지정 24채널 이하/13 us/24 kHz ±5%로 변경했다.',
            '시험2의 24채널/13 us는 공통 가정 H35에 두고 조건별 1~24채널과 950~1050 Hz를 유지했다.',
            '전류의 계산식과 백분율 산식은 줄였지만 설정값 500/1000/1500 uA 및 각 ±10% 경계는 유지했다.',
            '펄스폭 13/25/100/255 us와 ±5% 경계는 각 조건에 그대로 두었다.',
            '대역/맵/볼륨 개수 등 조건 공통 숫자는 같은 시험의 준비/가정/다른 조건에 유지했다.',
            'LED 알림 1은 허용/켜짐으로 풀었다. 잔량 80/79/10/9%, 충전 연결/해제와 색상은 유지했다.',
            '조건별 입력/응답 배열은 전후 자동 비교에서 누락이 없었다. RTT 전체 문자열도 유지했다.',
            'CFX/CM3 보조 식별자 등 반복 설명과 시간 계산식의 기호를 줄였다. 요구사항 E/F열은 변경하지 않았다.',
            '시험58의 처리 주기 0.95~1.05 ms와 다음 시작 전 완료 조건은 유지하고 수식 대신 문장으로 적었다.',
            'J242의 중간 단계 수/시간 미정 설명은 삭제했고 단계적 증가 판정 J239는 보존했다.',
        ],
        'noNewMeasurements':True,
        'inputArraysUnchanged':True,
    }
    write_json('content-review.json',summary)


def fit():
    data=json.loads((QA/'plan.json').read_text('utf8'))
    measurements=[]
    for kind in ['system','criterion']:
        measurements.extend(json.loads((QA/f'{kind}-native-check.json').read_text('utf-8-sig'))['measurements'])
    # 세로 병합은 보존하고 각 행의 단독 셀과 병합 영역 모두 필요한 높이를 확보한다.
    heights={(MAIN,r):36.0 for r in range(34,282)}
    heights.update({(x['sheet'],x['row']):x['height'] for x in data['heights'] if x['sheet']!=MAIN})
    snapshot=reader.extract(QA/'system-before.xlsx')
    def bounds(item):
        sheet=MAIN if item['sheet']==CRITERION else item['sheet']; row=int(re.search(r'\d+',item['cell']).group())
        for merged in snapshot['sheets'][sheet]['merges']:
            a,b=merged.split(':'); first,last=int(re.search(r'\d+',a).group()),int(re.search(r'\d+',b).group())
            if a==item['cell']: return sheet,first,last
        return sheet,row,row
    for item in measurements:
        sheet,first,last=bounds(item)
        if sheet!=MAIN or not 34<=first<=281 or first!=last: continue
        heights[(sheet,first)]=max(heights[(sheet,first)],item['needed']+3)
    for item in measurements:
        sheet,first,last=bounds(item)
        if sheet==MAIN and 34<=first<=last<=281:
            have=sum(heights[(sheet,r)] for r in range(first,last+1)); need=item['needed']+3
            if have<need: heights[(sheet,first)]+=need-have
        elif item['needed']>item['current']+1:
            heights[(sheet,first)]=max(heights.get((sheet,first),0),item['rowHeight']+item['needed']-item['current']+3)
    assert max(heights.values())<=409.5
    data['heights']=[{'sheet':s,'row':r,'height':h} for (s,r),h in sorted(heights.items())]
    write_json('plan.json',data)
    print('Height rows:',len(heights),'maximum:',max(heights.values()))


def verify(published=False):
    data=json.loads((QA/'plan.json').read_text('utf8')); baseline=json.loads((QA/'baseline.json').read_text('utf8'))
    outcomes={}
    for kind,record in baseline['files'].items():
        path=Path(record['path']) if published else QA/f'{kind}-candidate.xlsx'
        old,new=reader.extract(QA/f'{kind}-before.xlsx'),reader.extract(path)
        assert list(old['sheets'])==list(new['sheets'])
        edits=previous.expected_changes(kind,data)
        expected={(CRITERION if kind=='criterion' and x['sheet']==MAIN else x['sheet'],x['cell']):x['after'] for x in edits}
        heights={(CRITERION if kind=='criterion' and x['sheet']==MAIN else x['sheet'],x['row']):x['height'] for x in data['heights'] if kind=='system' or x['sheet'] in [MAIN,CYBER]}
        actual=[]
        for name,sheet in old['sheets'].items():
            target=new['sheets'][name]
            assert sheet['dimension']==target['dimension'] and sheet['merges']==target['merges']
            for a in sheet['cells'].keys()|target['cells'].keys():
                before,after=sheet['cells'].get(a,''),target['cells'].get(a,'')
                if before!=after:
                    assert (name,a) in expected,('unexpected',kind,name,a)
                    assert after==expected[(name,a)]
                    actual.append((name,a))
                if name==MAIN and re.fullmatch(r'[KL]\d+',a) and a not in ['K34','L34']:
                    assert before==after,('result changed',name,a)
        assert len(actual)==len(expected)
        before_pack,after_pack=previous.native.parts(QA/f'{kind}-before.xlsx'),previous.native.parts(path)
        assert before_pack.keys()==after_pack.keys()
        changed_parts={previous.native.sheet_path(after_pack,n) for n,_ in expected.keys()|heights.keys()}
        for part,content in before_pack.items():
            if kind=='system' and part=='xl/media/aligned_system_image.png':
                assert after_pack[part]==(QA/'SYS-01-rate-24k.png').read_bytes()
                continue
            if part not in changed_parts:
                assert content==after_pack[part],('package changed',part)
                continue
            roots=[previous.native.parse(content),previous.native.parse(after_pack[part])]
            for (name,a),text in expected.items():
                if previous.native.sheet_path(after_pack,name)!=part: continue
                for root in roots:
                    cell=root.find(f'.//s:c[@r="{a}"]',previous.native.N)
                    for child in list(cell): cell.remove(child)
                    cell.attrib.pop('t',None)
            for (name,row),height in heights.items():
                if previous.native.sheet_path(after_pack,name)!=part: continue
                for root in roots:
                    element=root.find(f's:sheetData/s:row[@r="{row}"]',previous.native.N)
                    if root is roots[1]: assert abs(float(element.get('ht'))-height)<0.01
                    element.attrib.pop('ht',None); element.attrib.pop('customHeight',None)
            assert previous.native.serialize(roots[0])==previous.native.serialize(roots[1]),('structure/style',part)
        sheet=MAIN if kind=='system' else CRITERION
        old_test,new_test=tests_from(old,sheet),tests_from(new,sheet)
        assert [(t['id'],t['start'],t['end']) for t in old_test]==[(t['id'],t['start'],t['end']) for t in new_test]
        for t in new_test:
            for a,text in t['cells'].items():
                if a.startswith('J'):
                    assert re.findall(r'조건\s*\[\d+\]',old['sheets'][sheet]['cells'].get(a,''))==re.findall(r'조건\s*\[\d+\]',text)
        outcomes[kind]={'changedCells':len(actual),'ids':len(new_test),'sha256':reader.sha(path)}
    rpath=Path(baseline['files']['system']['path']) if published else QA/'system-candidate.xlsx'
    cpath=Path(baseline['files']['criterion']['path']) if published else QA/'criterion-candidate.xlsx'
    r,c=reader.extract(rpath),reader.extract(cpath)
    for rn,cn in [(MAIN,CRITERION),(CYBER,CYBER)]:
        rs,cs=r['sheets'][rn]['cells'],c['sheets'][cn]['cells']
        for a in rs.keys()|cs.keys():
            if re.fullmatch(r'[A-J]\d+',a) and a!='B2': assert rs.get(a,'')==cs.get(a,''),(rn,a)
    before=reader.extract(QA/'system-before.xlsx')
    condition_count=0
    qcc_positions={}
    for name in [MAIN,CYBER]:
        old_cells=before['sheets'][name]['cells']; new_cells=r['sheets'][name]['cells']
        positions=lambda cells: sorted(a for a,t in cells.items() if '(QCC 검증 결과 필요)' in t)
        assert positions(old_cells)==positions(new_cells)
        qcc_positions[name]=positions(new_cells)
        if name==MAIN:
            for a in old_cells.keys()|new_cells.keys():
                if not re.fullmatch(r'J\d+',a): continue
                old_conditions=re.findall(r'조건\s*\[\d+\]',old_cells.get(a,''))
                new_conditions=re.findall(r'조건\s*\[\d+\]',new_cells.get(a,''))
                assert old_conditions==new_conditions,(a,'condition missing')
                condition_count+=len(new_conditions)
    for path,digest in baseline['protected'].items(): assert reader.sha(Path(path))==digest,('protected',path)
    out={'files':outcomes,'conditions':condition_count,'qccPositions':qcc_positions,'protectedFiles':len(baseline['protected']),'commonColumnsEqual':True,'newBoardTests':False,'test1Criterion':'24 kHz ±5%, active channels <=24, pulse width 13 us','otherResultsPreserved':True}
    write_json('published-verification.json' if published else 'verification.json',out)
    print(json.dumps(out,ensure_ascii=False))


def publish():
    verify()
    baseline=json.loads((QA/'baseline.json').read_text('utf8'))
    last_publication=json.loads((QA/'published-verification.json').read_text('utf8')) if (QA/'published-verification.json').exists() else None
    for kind,record in baseline['files'].items():
        check=json.loads((QA/f'{kind}-native-check.json').read_text('utf-8-sig'))
        assert check['readOnly'] and check['hash']==reader.sha(QA/f'{kind}-candidate.xlsx')
        assert all(x['needed']<=x['current']+1 for x in check['measurements'])
        allowed={record['sha256']}
        if last_publication: allowed.add(last_publication['files'][kind]['sha256'])
        assert reader.sha(Path(record['path'])) in allowed,'External edit: preserve target.'
        with Path(record['path']).open('r+b'): pass
    for kind,record in baseline['files'].items(): shutil.copyfile(QA/f'{kind}-candidate.xlsx',record['path'])
    verify(True)


if __name__ == '__main__':
    {'prepare':prepare,'parent':parent_proposals,'plan':plan,'draw':draw_rate,'audit':audit_content,'build':build,'fit':fit,'verify':verify,'publish':publish}[sys.argv[1]]()
