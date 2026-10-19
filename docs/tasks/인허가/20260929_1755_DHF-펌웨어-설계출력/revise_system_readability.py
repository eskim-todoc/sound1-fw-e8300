"""시스템 시험 설명/절차를 쉽게 풀고 보고서와 기준서의 공통 값을 맞춘다."""
import copy
import json
import re
import shutil
import sys
from pathlib import Path

import inspect_system_cybersecurity as reader
import preserve_aligned_report_packages as native
import preserve_system_cybersecurity as bridge

TASK = Path(__file__).resolve().parent
QA = TASK / 'qa/system-readable-rev0'
MAIN = '보고서_시스템 시험'
CRITERION = '기준서_시스템 시험'
HELPER = '시험 보조자료'
CYBER = '사이버보안 시험 연계'
WRAPPED_EQUIPMENT = {'C18', 'D18'}


def equipment_wrap_styles(package, sheet_name):
    """장비 설명 두 셀만 줄바꿈을 켠다. 다른 셀의 공용 스타일은 수정하지 않는다."""
    styles = native.parse(package['xl/styles.xml'])
    xfs = styles.find('s:cellXfs', native.N)
    root = native.parse(package[native.sheet_path(package, sheet_name)])
    cells = {x.get('r'): x for x in root.findall('.//s:c', native.N)}
    mapping = {}
    for address in sorted(WRAPPED_EQUIPMENT):
        original = int(cells[address].get('s'))
        cloned = copy.deepcopy(xfs[original])
        alignment = cloned.find('s:alignment', native.N)
        assert alignment is not None
        alignment.set('wrapText', '1')
        mapping[address] = (original, len(xfs))
        xfs.append(cloned)
    xfs.set('count', str(len(xfs)))
    package['xl/styles.xml'] = native.serialize(styles)
    return mapping


def padded(text):
    return '\n' + text.strip() + '\n'


def terminology(text):
    """측정 기록은 바꾸지 않고 PBS를 지그의 이름으로 쓴 부분만 바로잡는다."""
    replacements = [
        ('내부기 출력 측정 PBS 지그에 결합된 내부기', '내부기 출력 측정용 지그에 연결하고 전극을 PBS 용액에 담근 내부기'),
        ('내부기 출력 측정 PBS 지그에 연결된 오실로스코프', '내부기 출력 측정용 지그에 연결된 오실로스코프'),
        ('내부기 출력 측정 PBS 지그', '내부기 출력 측정용 지그'),
        ('내부기 출력 측정 지그', '내부기 출력 측정용 지그'),
        ('PBS 지그', '내부기 출력 측정용 지그'),
        ('PBS 저항', '전류 측정에 사용하는 지그의 측정 저항'),
        ('PBS 부하 R', '지그의 전류 측정 저항 R'),
        ('PBS 부하전압', '지그에서 측정한 전압'),
        ('PBS 부하 전압', '지그에서 측정한 전압'),
        ('PBS 전극', 'PBS 용액에 담근 전극'),
        ('PBS의 모든 활성 전극', 'PBS 용액에 담근 모든 활성 전극'),
        ('PBS의 해당 전극', 'PBS 용액에 담근 해당 전극'),
        ('PBS 스코프', '오실로스코프'),
        ('PBS 첫 위상', '전극 출력의 첫 위상'),
        ('PBS 양 위상', '전극 출력의 두 위상'),
        ('PBS 자극 출력', '지그에서 측정한 자극 출력'),
        ('PBS 자극 파형', '전극의 자극 파형'),
        ('PBS 연속 파형', '전극의 연속 자극 파형'),
        ('PBS 알림 파형', '전극의 알림용 자극 파형'),
        ('PBS 진폭 파형', '전극의 자극 크기 파형'),
        ('PBS 파형', '전극의 자극 파형'),
        ('전극의 전극의 자극 파형', '전극의 자극 파형'),
        ('전극-기준전극 전극의 자극 파형', '전극-기준전극 사이 자극 파형'),
        ('중간 입력 예시으로', '중간 입력 예시로'),
        ('맵 2을', '맵 2를'),
        ('맵 4을', '맵 4를'),
        ('볼륨=2을', '볼륨=2를'),
        ('볼륨=4을', '볼륨=4를'),
        ('볼륨 5을', '볼륨 5를'),
        ('SW-FW-SYS-Test-ID-2은', 'SW-FW-SYS-Test-ID-2는'),
        ('SW-FW-SYS-Test-ID-4은', 'SW-FW-SYS-Test-ID-4는'),
        ('SW-FW-SYS-Test-ID-5은', 'SW-FW-SYS-Test-ID-5는'),
        ('·', ', '),
    ]
    for before, after in replacements:
        text = text.replace(before, after)
    return text


# 보안 항목의 적용 범위와 수치는 그대로 두고, 무엇을 확인하는지 풀어 쓴다.
CYBER_WORDS = {
    5: (
        '1. 맵에 등록된 내부기와 등록되지 않은 내부기를 각각 연결한다.\n'
        '2. 블루투스 연결 대상을 등록하지 않은 상태, 등록(페어링)만 완료한 상태, 추가 인증(CRA)까지 완료한 상태에서 같은 제어 명령을 보낸다.',
        '내부기: 맵에 등록된 내부기만 연결되어야 한다.\n'
        'QCC: 추가 인증 전에는 일반 제어 명령을 거부하고, 인증 완료 후에는 허용해야 한다.'),
    6: ('사용자 계정을 만들거나 삭제하는 기능이 있는지 설계 문서와 제품의 설정 화면을 확인한다.',
        '사용자 계정 관리 기능이 없다는 설계 내용과 이 보안 항목을 적용하지 않는 사유가 일치해야 한다.'),
    7: ('1. 맵에 등록된 내부기의 식별번호를 조회한다.\n2. QCC가 블루투스 연결 대상을 구분할 때 사용하는 식별정보를 확인한다.',
        '각 연결 대상의 식별정보를 일관되게 사용해야 한다.\n등록된 대상과 다른 대상을 구분할 수 있어야 한다.'),
    8: ('1. 생산 시 QCC 인증키를 설정하고 저장하는 자료를 확인한다. 인증키는 연결 상대를 확인하기 위한 비밀정보이다.\n'
        '2. 키의 저장, 사용, 전송 과정에서 보호하는 방법과 키 변경 정책을 확인한다.\n3. 내부기 정보 파일을 암호화하여 저장하는 처리도 확인한다.',
        '인증정보를 저장, 사용, 전송할 때의 보호 방법이 확인되어야 한다.\n초기 키 설정과 키 변경 정책이 제품의 사용 방식에 맞게 정해져 있어야 한다.'),
    9: ('제품이 사용자 설정 비밀번호로 인증하는지, 기기에 저장된 인증키로 연결 상대를 확인하는지 구분한다.',
        '사용자 비밀번호 기능이 없다면 이 항목을 적용하지 않는 사유가 성립해야 한다.\n기기 인증키의 보호는 IA-04 및 DC-03에서 확인해야 한다.'),
    10: ('1. 올바른 인증정보와 잘못된 인증정보를 각각 입력한다.\n2. 인증 성공, 실패, 진행 중 및 시간 초과 시의 응답과 오류 메시지를 확인한다.',
        '화면이나 응답에는 인증 결과만 표시되어야 한다.\n인증키 원문이나 인증정보를 알아낼 수 있는 단서가 노출되지 않아야 한다.'),
    11: ('1. 인증을 연속 5회 실패시킨다.\n2. 추가 인증 시도가 차단되는지 확인한다.\n3. 설정된 제한 시간이 지난 후 올바른 정보로 다시 인증한다.\n4. 제한 중에도 필요한 자극 기능에 어떤 영향이 있는지 확인한다.',
        '설정된 제한 시간 동안 추가 인증 시도가 차단되어야 한다.\n제한이 해제된 후에는 정상 인증이 가능해야 한다.\n인증 제한이 자극의 필수 기능에 미치는 영향도 확인되어야 한다.'),
    12: ('1. 리모콘 및 매핑 프로그램 연결 전후의 상태 표시를 확인한다.\n2. 인증 화면의 시스템 사용 알림과 그 알림을 설정할 수 있는 사람의 권한을 확인한다.',
        '연결 상태 표시와 시스템 사용 알림을 구분해야 한다.\n알림이 필요한 화면에는 해당 메시지가 표시되어야 한다.\n허가된 사람만 알림을 설정할 수 있어야 한다.'),
    13: ('1. 인증 전과 인증 후에 같은 일반 제어 명령 및 매핑 명령을 보낸다.\n'
         '2. E8300의 리모콘 명령 처리 허용 상태를 false(처리 불허)와 true(처리 허용)로 각각 설정하여 응답을 확인한다.',
         '인증 전 일반 제어 명령은 거부되어야 한다.\n'
         'E8300 처리 불허(false): 기능을 실행하지 않고 오류 응답의 헤더=0xF0, 오류코드=2를 보내야 한다. 처리 중 명령은 IDLE(대기)로 정리되어야 한다.\n'
         'E8300 처리 허용(true): 요청한 기능만 실행되어야 한다.'),
    14: ('외부에서 받은 실행 파일이나 스크립트를 제품에서 실행할 수 있는지 확인한다.\n무선 업데이트(OTA)로 받는 고정 펌웨어 파일은 별도의 업데이트 항목에서 확인한다.',
         '외부에서 내려받은 스크립트 등 실행 코드(모바일 코드)를 실행하는 기능이 없어야 한다. 여기서 모바일 코드는 스마트폰 앱을 뜻하지 않는다.\n펌웨어 업데이트의 통제는 SI-08 및 SI-09에서 확인해야 한다.'),
    15: ('1. 인증을 완료한 뒤 제어 명령을 더 보내지 않는다.\n2. 기존 기준의 관찰시간 30분 동안 블루투스 연결이 종료되는지 확인한다.\n3. 연결 종료 후 다시 접속한다.',
         'QCC에 설정된 무조작 조건이 되면 블루투스 연결이 해제되어야 한다.\n재접속에는 필요한 인증이 다시 적용되어야 한다.\n이 시험의 연결 종료는 내부기 연결 중의 절전 동작과 구분해야 한다.'),
    16: ('1. 블루투스를 연결한 뒤 해제한다.\n2. 설정 파일이 손상된 조건을 만든다.\n3. 각 상황을 시간 및 식별정보와 함께 저장하는 감사기록을 확인한다.',
         '연결 시 이벤트 코드=0x40, 해제 시=0x32, 설정 파일 검사 오류 시=0xF0가 기록되어야 한다.\n각 기록에 관련 시간과 식별정보가 함께 저장되어야 한다.'),
    17: ('감사기록 파일의 열기, 읽기, 쓰기를 각각 실패시켜 오류 처리 결과를 확인한다.\n파일 동기화, 닫기 및 저장공간 부족은 기존 열기/읽기/쓰기 시험과 구분해 확인한다.',
         '기존 시험 범위인 열기, 읽기, 쓰기 실패: 반환값=-1이어야 하며 감사 오류 정보로 실패 종류를 구분할 수 있어야 한다.\n'
         '동기화, 닫기, 저장공간 부족 등 기존 시험에서 다루지 않은 오류의 처리와 필수 기능 영향은 별도로 확인되어야 한다.'),
    18: ('시간을 [26,10,2,13,14,15]로 설정한다. 순서대로 연, 월, 일, 시, 분, 초이다.\n이벤트를 기록한 후 저장된 날짜와 시간을 읽는다.',
         '저장된 연, 월, 일, 시, 분, 초가 입력값 [26,10,2,13,14,15]와 각각 일치해야 한다.'),
    19: ('사용자가 제어 또는 설정 변경을 수행한다.\n해당 사용자 식별정보, 수행한 행동 및 수행 시각이 감사기록에서 서로 연결되는지 확인한다.',
         '정해진 기록 범위에서 누가, 어떤 행동을, 언제 수행했는지 연결할 수 있어야 한다.\n장치 식별정보와 사용자 식별정보는 구분해서 확인해야 한다.'),
    20: ('1. QCC 인증 전후의 데이터 송수신과 연결 구간의 암호화 상태를 확인한다.\n'
         '2. 내부기와 주고받는 무선 데이터는 정상 데이터와 CRC 오류가 있는 데이터를 각각 입력한다. CRC는 데이터 손상 여부를 검사하는 값이다.',
         'QCC: 인증 및 암호화가 적용된 연결에서 허용된 데이터만 주고받아야 한다.\n내부기 무선 통신: CRC 오류 데이터가 정상 응답으로 처리되지 않아야 한다.'),
    21: ('1. 허가된 담당자가 펌웨어를 생성하고 배포하는 절차를 확인한다.\n'
         '2. 받은 펌웨어를 검사하고 실행 메모리(PRAM)에 적재한 뒤 잠그는 순서를 확인한다.\n3. CRC 오류가 있는 파일을 입력했을 때의 처리 결과를 확인한다.',
         '허가된 배포 절차를 거친 펌웨어를 검사한 뒤 실행하는 경로가 확인되어야 한다.\n적재 후 실행 메모리를 잠가야 한다.\nCRC 오류 파일은 정해진 오류 처리 경로로 처리되어야 한다.'),
    22: ('인증 성공/실패, 펌웨어 파일의 CRC 정상/오류 및 AES 암호화 방식의 공개 복호화 입력값에 대한 기존 시험 결과를 확인한다.',
         '각 보안 기능이 정상 조건과 오류 조건을 구분해야 한다.\n'
         'AES 복호화 시험의 4워드 입력: 출력=[6BC1BEE2,2E409F96,E93D7E11,7393172A], 반환값=0이어야 한다.'),
    23: ('펌웨어 파일의 헤더, 일반 데이터 구획 및 설정 파일의 내용을 변경한다.\n변경 전 CRC 값은 그대로 둔 상태에서 각 파일을 읽거나 적재한다.',
         '펌웨어 일반 구획: CRC 오류가 있으면 정상 적재로 처리되지 않아야 한다.\n설정 파일: 손상이 검출되면 정해진 오류 처리 또는 초기화 경로로 처리되어야 한다.'),
    24: ('정상 명령, 지원하지 않는 명령, OTA 파일 종류 5, 저장 위치(슬롯) 3, 짧은 데이터 및 잘못된 길이의 데이터를 각각 입력한다.',
         'OTA 읽기(READ) 응답에서 확인할 두 항목은 [응답헤더,오류코드]이다.\n'
         '슬롯 3 입력: [0xF0,3]이어야 한다.\n파일 종류 5 입력: [0xF0,4]이어야 한다.\n'
         '위 두 항목은 전체 응답 데이터가 아닌 비교 대상 부분이다.\n각 명령에서 데이터 길이와 허용 값 범위의 검사가 확인되어야 한다.'),
    25: ('연결 해제, 설정 파일 오류, 전원 켜기/끄기 및 대체 펌웨어로 기동하지 못하는 조건을 각각 확인한다.',
         '기동 및 종료 과정에서 자극을 금지한 구간에는 출력이 없어야 한다.\n오류가 발생하면 정해진 상태로 전환되어야 한다.\n'
         '대체 저장 위치의 기동 시도 횟수가 상한을 넘으면 기본 저장 위치인 슬롯 1로 돌아가야 한다.'),
    26: ('정상 명령, 지원하지 않는 명령 및 인증 오류를 각각 입력한다.\n제품의 오류 응답과 표시 내용을 확인한다.',
         '정해진 오류 응답으로 처리되어야 한다.\n인증키와 중요정보가 오류 메시지에 노출되지 않아야 한다.'),
    27: ('1. 인증 및 암호화가 적용된 QCC 연결로 무선 업데이트 데이터를 받는다.\n'
         '2. 지원하는 파일 종류 1, 2, 3, 4 및 저장 위치(슬롯) 1, 2를 입력한다.\n'
         '3. 지원하지 않는 파일 종류 5 및 슬롯 3을 입력하고 오류 응답과 이후 파일 처리 여부를 각각 확인한다.\n4. 다시 기동하여 받은 파일의 검사와 적재 결과를 확인한다.',
         '통신 구간: QCC 인증 및 암호화로 보호되어야 한다.\n'
         'OTA 읽기(READ) 응답의 비교 대상 [응답헤더,오류코드]: 슬롯 3은 [0xF0,3], 파일 종류 5는 [0xF0,4]이어야 한다.\n'
         '오류 응답을 보낸 결과와 이후 파일 처리를 차단한 결과를 각각 확인해야 한다.\n'
         '헤더와 일반 데이터 구획: CRC 검사가 정상인 경우에 적재해야 한다.\n'
         'VOLATILE 구획의 CRC 검사 예외와 필요한 구획이 빠진 경우는 해당 처리 범위를 별도로 대조해야 한다.'),
    28: ('허가된 절차로 배포한 파일을 인증 및 암호화가 적용된 QCC 연결로 받는다.\n부트로더가 파일을 AES 방식으로 복호화하고 CRC를 검사하는 처리를 확인한다.',
         '파일은 인증된 연결로 받아야 한다.\nCRC 검사 잔여값=0인 입력: 반환값=0이어야 한다.\n'
         '복호화 후 CRC 오류가 있는 입력: 반환값=-1이어야 한다.\n'
         '무선 통신 구간의 보호와 저장 파일이 정식 배포 파일인지 확인하는 범위는 구분해야 한다.'),
    29: ('양산품에서 프로그램 기록/디버그 단자에 접근하는 방법과 서비스 기능의 진입 조건을 확인한다.\n펌웨어 적재 후 실행 메모리(PRAM)를 잠그는 순서를 확인한다.',
         '허가되지 않은 물리 접근과 파일 변경을 제한하는 조건이 확인되어야 한다.\n실행 메모리 잠금은 파일 적재가 완료된 후 적용되어야 한다.'),
    30: ('정상 파일과 손상된 파일을 각각 사용하여 기동한다.\n펌웨어 파일의 검사/실행 결과와 설정 파일의 초기화 결과를 확인한다.',
         '정상 CRC 펌웨어: 실행되어야 한다.\n일반 데이터 구획에 오류가 있는 펌웨어: 거부되어야 한다.\n'
         '설정 파일 오류: 기본값 [1,4,1,1,1,2,1]이 적용되어야 한다.'),
    31: ('QCC 연결 구간에서 암호화가 적용되는지 확인한다.\n내부기 제조번호와 사용자 이니셜을 파일에 암호화해 저장한 뒤 다시 읽어 원래 정보로 복호화하는지 확인한다.',
         '전송 구간이 암호화되어야 한다.\n내부기 제조번호와 사용자 이니셜은 AES 방식으로 저장 및 복호화되어야 한다.'),
    32: ('저장하는 내부기 제조번호 및 사용자 이니셜 등의 항목을 확인한다.\n그 정보로 환자를 알아볼 수 있는지, 알아볼 수 있다면 개인을 식별하지 못하도록 처리하는 방법이 있는지 확인한다.',
         '개인을 식별하는 정보를 저장하지 않거나, 저장한다면 해당 정보의 비식별화 방법이 확인되어야 한다.\n'
         '개인을 알아볼 수 없도록 처리하는 비식별화와 정보 내용을 암호로 바꾸는 AES 암호화는 구분해서 확인해야 한다.'),
    33: ('1. QCC 통신과 인증키를 보호하는 방법을 확인한다.\n'
         '2. E8300의 저장 파일 및 부트 파일에 적용하는 AES 암호화의 방식, 모드 및 키 길이를 확인한다.\n3. CRC가 무엇을 검사하는지 확인한다.',
         'E8300: AES-128 ECB 방식, 16바이트 블록 처리가 확인되어야 한다.\nCRC16: 데이터 오류 검출 용도로 구분되어야 한다.\n'
         'QCC 보호 방식: 적용 목적에 맞는 권고 근거가 별도로 확인되어야 한다.'),
    34: ('감사기록을 읽고 전송할 수 있는 권한과 전송 시 암호화 여부를 확인한다.\n'
         '허가된 사용자도 저장된 기록을 수정할 수 있는지 확인한다.\n제조/시험 중의 직접 접근과 원격 접근은 구분해서 확인한다.',
         '허가된 경로에서만 감사기록을 읽을 수 있어야 한다.\n전송 시 기록이 보호되어야 한다.\n허가된 사용자도 저장된 감사기록을 수정할 수 없어야 한다.'),
    35: ('지원하지 않는 명령, 잘못된 입력 및 연결 해제에 대한 기존 결과를 확인한다.\n'
         '잘못된 입력을 반복하거나 지나치게 많이 보내 제품 사용을 방해할 위험을 검토한다.',
         '기존 오류 입력이 정상 제어 명령으로 처리되지 않아야 한다.\n반복 입력이 자극의 필수 기능을 방해하는지 평가되어야 한다.'),
    36: ('기본 펌웨어 저장 위치(슬롯 1), 대체 위치(슬롯 2) 및 사용자 설정의 기본값 복구를 확인한다.\n백업 대상 자료와 복구 방법을 확인한다.',
         '정해진 펌웨어 및 설정의 복구 경로가 확인되어야 한다.\n백업하지 않는 자료는 그 사유와 복구 방법이 명확해야 한다.'),
    37: ('대체 펌웨어 기동의 시도 상한을 3으로 둔다.\n호출 전 시도 횟수 2와 3을 각각 입력한다.\n사용자 설정도 초기화한다.',
         '호출 전 횟수 2: 호출 후 3이 되며 슬롯 2를 유지해야 한다.\n'
         '호출 전 횟수 3: 호출 후 4가 되며 슬롯 1로 돌아가야 한다. 상태값은 BOOT/TRY_DONE/FAIL이어야 한다.\n'
         '사용자 설정 초기화: 기본값 [1,4,1,1,1,2,1]이어야 한다.'),
    38: ('사용설명서의 연결 대상 등록(페어링) 및 보안 설정 방법을 실제 제품 설정과 비교한다.\n설정을 변경할 수 있는 사람의 권한과 변경 후 상태를 확인하는 방법을 검토한다.',
         '사용설명서에 해당 설정 방법이 안내되어 있어야 한다.\n허가된 설정 변경 방법과 변경 후 상태 확인 방법이 제품의 동작과 일치해야 한다.'),
    39: ('양산 설정에서 서비스/디버그 기능과 외부 접근 단자를 확인한다.',
         '제품 사용에 필요한 기능만 사용할 수 있어야 한다.\n제조/시험 기능은 접근이 제한되거나 사용하지 않도록 설정되어 있어야 한다.'),
}


def prepare():
    QA.mkdir(parents=True, exist_ok=True)
    old = json.loads((TASK / 'qa/system-advertisement-gate/baseline.json').read_text('utf8'))
    baseline = {'files': {}, 'protected': {p: reader.sha(Path(p)) for p in old['protected']}}
    for kind, item in old['files'].items():
        path = Path(item['path'])
        backup = QA / f'{kind}-before.xlsx'
        if backup.exists():
            assert reader.sha(backup) == reader.sha(path), 'Snapshot already exists with different input.'
        else:
            shutil.copy2(path, backup)
        baseline['files'][kind] = {'path': str(path), 'sha256': reader.sha(path)}
        data = reader.extract(path)
        (QA / f'{kind}-before.json').write_text(json.dumps(data, ensure_ascii=False, indent=2), 'utf8')
    data = reader.extract(QA / 'system-before.xlsx')
    cells = data['sheets'][MAIN]['cells']
    starts = sorted((int(a[1:]), int(v.strip().rsplit('-', 1)[1])) for a, v in cells.items()
                    if re.fullmatch(r'B\d+', a) and re.fullmatch(r'SW-FW-SYS-Test-ID-\d+', v.strip()))
    tests = []
    for i, (row, test_id) in enumerate(starts):
        end = starts[i + 1][0] - 1 if i + 1 < len(starts) else 281
        content = {a: v for a, v in cells.items() if row <= int(re.search(r'\d+', a).group()) <= end}
        tests.append({'id': test_id, 'row': row, 'end': end, 'cells': content})
    assert len(tests) == 81
    (QA / 'tests.json').write_text(json.dumps(tests, ensure_ascii=False, indent=2), 'utf8')
    (QA / 'baseline.json').write_text(json.dumps(baseline, ensure_ascii=False, indent=2), 'utf8')
    print('Snapshotted two workbooks / 81 tests / protected sources.')


def plan():
    source = reader.extract(QA / 'system-before.xlsx')
    tests = json.loads((QA / 'tests.json').read_text('utf8'))
    changes = {}
    issues = []

    def put(sheet, cell, text, reason, pad=None):
        old = source['sheets'][sheet]['cells'].get(cell, '')
        if pad is None:
            pad = sheet == MAIN and old.startswith('\n')
        text = terminology(text.strip())
        text = padded(text) if pad else text
        if text == old:
            changes.pop((sheet, cell), None)
        else:
            changes[(sheet, cell)] = {'sheet': sheet, 'cell': cell, 'after': text, 'reason': reason}

    reviewed = []
    for filename in ['proposals-1-32.json', 'proposals-33-81.json']:
        item = json.loads((QA / filename).read_text('utf8'))
        reviewed.extend(item['reviewed_ids'])
        issues.extend(item.get('issues', []))
        for change in item['changes']:
            assert re.fullmatch(r'[CGHIJ]\d+', change['cell']), change
            assert int(change['cell'][1:]) >= 34
            put(MAIN, change['cell'], change['after'], change['reason'])
    assert sorted(reviewed) == list(range(1, 82))
    assert all((MAIN, f'C{x["row"]}') in changes for x in tests), 'Every test needs a plain purpose.'

    def current(sheet, cell):
        return changes.get((sheet, cell), {}).get('after', source['sheets'][sheet]['cells'].get(cell, ''))

    # 공통 장비는 기존 셀 안에서 설명하며 행/열/시험 번호는 추가하지 않는다.
    put(MAIN, 'C18', '内部기 출력 측정용 지그 및 PBS 용액\n또는 내부기 테스트 지그(32채널 핀 헤더)', '두 측정 구성의 장비 이름 구분')
    put(MAIN, 'D18', '실제 내부기는 출력 측정용 지그에 연결하고 전극을 PBS 용액에 담근다.\n'
        '32채널 핀 헤더가 있는 내부기 테스트 지그는 핀 헤더에서 오실로스코프로 바로 측정한다.', 'PBS는 용액이며 자극 발생기는 내부기임을 명확화')

    put(MAIN, 'C38', '선택한 자극 전극과 공통 기준전극 사이에 전류를 흘리는 방식(모노폴라)으로 실제 자극이 나오는지 확인한다.', '모노폴라의 동작을 쉽게 설명')
    put(MAIN, 'C44', '한 번의 자극에서 음극과 양극 펄스가 각각 이어지는 시간이 설정한 펄스폭과 허용 범위에 맞는지 확인한다.', '위상과 펄스폭의 측정 대상을 설명')
    put(MAIN, 'C78', '[E8300/QCC 각각 확인, 향후 I2S] 두 마이크 입력과 QCC가 디지털 방식으로 전달하는 소리(I2S)를 구별해 처리하고, 해당 소리에 따른 자극이 나오는지 확인한다.', 'I2S 입력이 무엇인지 설명')
    put(MAIN, 'C86', '자극 횟수, 반복 주기, 형태, 방향, 전류, 펄스폭 및 전하량 시험의 결과를 함께 확인하여 맵 설정에 따른 자극 기능이 기준을 만족하는지 확인한다.', '시험 ID 범위만으로 설명하지 않고 확인 내용을 열거')
    for row, number, channels in [(35, 1, list(range(1, 9))), (36, 2, list(range(9, 17))), (37, 3, list(range(17, 25)))]:
        put(MAIN, f'J{row}', f'조건 [{number}] 24채널 맵, 채널당 펄스폭 13 us\n'
            f'측정 채널: {", ".join(map(str, channels))}\n각 채널을 따로 측정하여 다음 기준과 비교한다.\n\n'
            '목표 반복률: 채널마다 1,000 Hz\n허용 하한: 950 Hz\n허용 상한: 1,050 Hz\n'
            '위에 적힌 모든 채널의 반복률이 하한 이상, 상한 이하일 때 통과한다(허용오차 ±5%).', '채널 목록과 반복률 하한/상한을 명확히 분리')
    for row, number, width, lower, upper in [(44, 1, 13, '12.35', '13.65'), (45, 2, 25, '23.75', '26.25'), (46, 3, 100, '95.00', '105.00'), (47, 4, 255, '242.25', '267.75')]:
        text = (f'조건 [{number}] 각 위상의 펄스폭 설정: {width} us\n전류, 전극 및 시험 맵을 확인한다.\n\n'
                f'목표 폭: {width} us\n허용 하한: {lower} us\n허용 상한: {upper} us\n'
                '음극 펄스와 양극 펄스의 폭을 각각 측정한다. 두 폭 모두 하한 이상, 상한 이하여야 한다(허용오차 ±5%).')
        if row == 44:
            text += '\n설정 가능한 위상 폭의 최솟값은 13 us, 최댓값은 255 us이다.'
        put(MAIN, f'J{row}', text, '위상별 펄스폭 하한/상한과 판정을 분리')
    for row in [50, 82]:
        text = ('소리를 입력할 재생 장치와 출력 소리를 들을 수신기를 따로 준비한다.\n'
                '실시간 소리 처리(LIVE)의 OD1+/OD1- 두 핀에는 PDM 디지털 오디오 신호를 받을 수 있는 출력 수신기를 연결한다.\n'
                '아날로그 출력으로 변환해 듣는 경우에는 실제 제작/확인된 변환 회로를 사용한다.')
        if row == 82:
            text = '100 Hz 및 8000 Hz 순음(한 주파수의 소리)을 출력할 발생기를 준비한다.\n' + text
        put(MAIN, f'G{row}', text, '입력 재생 장치와 PDM 출력 수신기를 구분해 간결하게 설명')
    put(MAIN, 'I50', '1. 같은 음원, 입력 레벨, 맵 및 자극 볼륨을 유지한다.\n'
        '2. 오디오 볼륨을 1, 5, 10으로 각각 변경하고 현재 설정값을 조회한다.\n'
        '3. 각 단계에서 OD1 출력 수신기의 소리를 듣고 출력 소리 크기가 달라지는지 확인한다.', '실제 청취할 출력과 설정 확인 절차만 설명')
    put(MAIN, 'I82', '1. 발생기로 100 Hz 순음을 입력하고 OD1 출력 수신기에서 소리가 들리는지 확인한다.\n'
        '2. 같은 입력 경로로 8000 Hz 순음을 입력하고 출력 수신기에서 해당 소리가 들리는지 확인한다.\n'
        '3. 각 입력 주파수와 청취 결과를 기록한다.', '입력 및 실제 청취 대상만 단계별로 설명')
    put(MAIN, 'I68', '1. 실시간 소리 처리를 유지한다. CFX가 오디오 한 묶음의 처리를 시작하는 시각을 마이크 입력 복사 직전에 같은 하드웨어 타이머로 저장한다.\n'
        '2. 연속한 시작 시각의 차이 P[k]=s[k+1]-s[k]를 계산한다.\n'
        '3. 수집이 끝난 뒤 RTT로 저장된 순번과 시작 시각을 읽는다. 전체 블록 수, 수집 시간, 최솟값/최댓값 및 기준을 벗어난 개수를 확인한다.\n'
        '4. 누락이나 버퍼 넘침 없이 기록되었는지 확인하고, 각 시작 간격을 통과 기준과 비교한다.\n'
        '시간 비교에는 장치에 저장한 시각을 사용한다.', '처리 주기 측정의 위치와 장치 내 시간 기준을 유지하며 코드명 나열을 제거')

    for item in tests:
        row = item['row']
        old_g = source['sheets'][MAIN]['cells'].get(f'G{row}', '')
        if 'PBS' in old_g:
            after = current(MAIN, f'G{row}').strip()
            if 'PBS 용액' not in after or '내부기 테스트 지그' not in after:
                # 같은 준비 조건에서 두 종류의 지그를 선택할 수 있음을 명시한다.
                after += ('\n측정 구성은 다음 중 하나를 사용한다.\n'
                          '- 실제 내부기: 출력 측정용 지그에 연결하고 전극을 PBS 용액에 담근다.\n'
                          '- 내부기 테스트 지그: 32채널 전극 핀 헤더에서 오실로스코프로 측정한다.')
            put(MAIN, f'G{row}', after, 'PBS 용액과 32채널 핀 헤더 지그의 사용 방법 구분')

    constraints = {
        'H34': '32개 활성 채널의 맵과 자극 순서, 펄스폭 및 자극률 설정을 기록한다.\n'
               '측정할 전극을 구분하고 파형을 확인할 시간을 정한다.\n'
               '설계 입력에는 20 kHz 이상과 24 kHz/±5% 조건이 함께 있다. 두 조건이 어느 시험에 적용되는지 추가 확인이 필요하다.',
        'H35': '활성 자극 채널이 24개 이하인 맵을 적용하고 실제 채널 수를 확인한다.\n'
               '설계 입력의 1000 Hz 이상 조건과 설계 출력의 1000 Hz ±5% 조건은 같지 않다.\n'
               '측정값을 두 기준과 각각 비교하고, 해당 시험의 측정 조건과 기준의 적용 관계를 확인한다.',
        'H38': '한 자극 전극과 공통 기준전극 사이에 전류를 흘리는 모노폴라 방식의 맵을 적용한다.',
        'H39': '음극 또는 양극 중 어느 펄스를 먼저 출력하는지 확인할 수 있는 시험 맵을 적용한다.',
        'H48': '한 번의 자극에서 전달하는 전하량이 최대 허용값을 넘도록 설정한 시험 맵을 적용한다.',
        'H54': '32채널이 활성화된 맵을 사용한다.',
        'H70': '출력 차이를 구별할 수 있도록 서로 다른 데이터가 저장된 맵 4개를 준비한다.',
        'H114': '배터리 부족 시 알림용 자극을 출력하는 설정을 켠다.',
        'H190': 'QCC가 제어 명령을 받지 않는 상태를 판단하는 조건과 실제 연결 종료 시간 설정을 확인한다. 자료번호와 시간 단위도 확인한다.\n'
                '연결 상대 장치의 자동 상태 조회를 중지한다. 연결 유지용 데이터와 제어 명령은 구분한다.\n'
                '30분은 기존 절차의 관찰시간이다. 실제 연결 종료 조건은 QCC 설정 및 자료와 비교한다.',
        'H212': 'SW-FW-RID-72, SYS-RID-FW-21 및 QCC 통신 규칙을 비교하여 5회 실패 제한이 추가 인증(CRA)과 BLE 페어링 중 어디에 적용되는지 먼저 확인한다.\n'
                '실패 횟수를 세는 대상, 연속 실패 횟수의 초기화 조건, 제한 시간, 제한 시작 시점 및 해제 조건을 확인한다. 실제 시간을 관찰할 방법도 준비한다.\n'
                '적용 단계가 확인되지 않은 경우에는 시험 실행과 판정을 대기한다.',
        'H237': '배터리 부족 시 알림용 자극을 출력하는 설정을 켠다.',
        'H239': '오디오 볼륨을 고정하고 자극 볼륨을 낮은 단계로 설정한다.\n정상 시험 내부기를 사용한다.',
        'H244': '실시간 소리 처리를 멈추지 않고 시각을 잴 수 있는 장치의 하드웨어 타이머를 준비한다.\n'
                '연속한 오디오 묶음의 순번, 시작 시각 s 및 완료 시각 e를 저장할 버퍼를 준비한다.\n'
                '타이머 주파수, 측정할 수 있는 최소 시간, 최대 계수 후 처음으로 돌아가는 조건 및 수집 시간을 확인한다.\n'
                '버퍼가 가득 차 기록을 놓치는지 확인할 방법과 실제 기록 저장/읽기 방법을 시험 전에 확인한다. 계측이 준비되지 않은 경우에는 시험 준비 미완료로 남긴다.\n'
                '측정 중에는 매 묶음마다 RTT 메시지를 출력하거나 디버거로 처리를 정지하지 않는다.',
        'H247': '시험용으로 짧은 명령을 보낼 수 있는 디버깅용 리모콘 앱을 준비한다.',
    }
    for cell, text in constraints.items():
        put(MAIN, cell, text, '시험 제한 조건의 어려운 용어를 풀고 기준 적용 범위를 보존')

    put(MAIN, 'H41',
        '시험 내부기의 자극칩 규격과 개정판을 확인하고 허용 전류의 최솟값/최댓값을 기록한다.\n'
        '규격 범위의 양 끝값과 중간값에 대해 맵의 T/C 전류(uA), 변환 코드, 자극 모드, 전극 및 펄스폭을 정한다.\n'
        '전류 환산에는 지그에서 실제 확인한 측정 저항값 R을 사용한다. PBS는 전극을 담그는 용액이다.\n'
        '500/1000/1500 uA는 규격 범위 안의 추가 예시이며, 칩의 전체 전류 범위는 별도로 확인한다.', 'PBS 용액을 고정 저항으로 잘못 표현한 조건 정정')
    put(MAIN, 'I41',
        '1. 자극칩 규격의 최소, 중간 및 최대 전류를 차례로 설정한다.\n'
        '2. 지그의 전류 측정 방법과 측정 저항값 R[Ω]을 확인한다. 해당 저항에 걸리는 펄스 전압 V[V]를 측정한다.\n'
        '3. 같은 위상에서 규격에 정한 구간의 전압을 읽고, I[uA]=V[V]/R[Ω]×1,000,000으로 전류를 계산한다.\n'
        '4. 설정 전류, R, V, 환산 전류 및 오차율을 비교한다. 각 설정의 오차율이 10% 이하인지 확인한다.', '실제 측정 저항에서 전류를 환산하는 순서를 명시')
    for row, number, target, lower, upper in [(41, 1, 500, 450, 550), (42, 2, 1000, 900, 1100), (43, 3, 1500, 1350, 1650)]:
        text = (f'조건 [{number}] 추가 입력 예시\n전류 설정: {target} uA\n펄스폭: 25 us\n'
                '측정 저항 R: 지그에서 확인한 값(Ω)\n\n'
                f'허용 하한: {lower} uA\n허용 상한: {upper} uA\n'
                '측정 전류는 하한 이상이며 상한 이하여야 한다(±10%).\n'
                '측정 전류 I[uA]=V[V]/R[Ω]×1,000,000\n'
                f'오차율=|I-{target}|/{target}×100: 10% 이하여야 한다.')
        if row == 41:
            text += '\n\n자극칩 규격 범위의 최소/중간/최대 설정에서도 전류 범위와 오차율 기준을 확인해야 한다.'
        put(MAIN, f'J{row}', text, '전류 입력/하한/상한과 전류 환산 방법을 분리')

    put(MAIN, 'H64',
        'CIS 시험 맵과 기본 프로그램 맵 3번(NofM)을 각각 준비한다. 실제 저장된 맵 3번의 자극 기법이 NofM인지 확인한다.\n'
        '분석 대역과 전극의 대응, CIS 출력 순서 및 NofM의 선택 수 N/전체 대역 수 M을 확인한다.\n'
        'N=16, M=32 시험은 16개보다 많은 분석 대역에 서로 다른 크기의 소리가 포함된 실제 음원을 사용한다.\n'
        '음원과 재생 레벨을 기록하고 정상 처리 후 각 대역의 자극 크기를 확인한다. 아래 고정 숫자 배열은 선택/순서 계산을 비교하는 보조 입력이다.', '맵 3번 및 실제 음원 시험과 디지털 보조 입력 구분')
    put(MAIN, 'I64',
        '1. CIS 맵을 선택하고 소리를 입력한다. 실제 전극의 자극 순서가 맵 설정과 같은지 측정한다.\n'
        '2. 기본 프로그램 맵 3번(NofM)을 선택한다. 16개보다 많은 분석 대역에 크기가 다른 소리를 입력한다.\n'
        '3. 정상 소리 처리 후 각 대역의 크기를 확인하여 선택되어야 할 16개 대역과 그 전극을 정한다.\n'
        '4. 각 전극에서 자극 유무, 크기 및 순서를 측정한다. 선택 전극에는 자극이 있고 미선택 전극에는 자극이 없는지 확인한다.\n'
        '5. 선택된 16채널은 1 ms마다 8채널씩, 두 묶음(2 ms)으로 출력되는 순서를 비교한다.\n'
        '자극 크기 배열을 직접 고정하는 검사는 선택 및 패킷 생성의 보조 확인이며, 실제 소리 입력 시험과 구분한다.', '맵 3번 선택부터 실제 전극 확인까지의 절차와 출력 주기 명시')
    put(MAIN, 'C64', 'CIS는 맵에 정한 순서대로 자극하는지 확인한다.\n기본 맵 3번(NofM)은 32개 분석 대역 중 소리 크기에 따라 16개를 선택하고 정해진 순서대로 자극하는지 확인한다.', '맵 3번의 16/32 선택 원리를 목적 설명에 명시')
    for row in [65, 66]:
        text = current(MAIN, f'J{row}').strip()
        text = text.replace('N-of-M', 'NofM')
        text = text.replace('기본 프로그램 맵 3번(NofM), NofM의 채널 선택을 확인하는 추가 디지털 입력 예시로',
                            '기본 프로그램 맵 3번(NofM)의 채널 선택 계산에 사용할 보조 입력으로')
        if '맵 3번' not in text:
            text = text.replace(f'조건 [{row-63}]', f'조건 [{row-63}] 기본 프로그램 맵 3번(NofM),', 1)
        text = text.replace('기본 프로그램 맵 3번(NofM), NofM의 채널 선택을 확인하는 추가 디지털 입력 예시로',
                            '기본 프로그램 맵 3번(NofM)의 채널 선택 계산에 사용할 보조 입력으로')
        if row == 66:
            text += '\n첫 묶음과 둘째 묶음은 각각 1 ms에 출력되며, 선택된 16채널 한 순서는 2 ms에 걸쳐 출력되어야 한다.'
        put(MAIN, f'J{row}', text, 'NofM 시험 맵과 8채널/1 ms 출력 순서를 명시')

    # 실제 출력 기준은 아래에 직접 적되, 준비/절차의 중복 경고는 덧붙이지 않는다.
    for cell in ['J50', 'J51', 'J52']:
        text = current(MAIN, cell)
        text = text.replace('\n입력용 음원 스피커의 변화나 PDM 디지털 신호의 사인파 모양으로 판정하지 않는다.', '')
        put(MAIN, cell, text, '출력 수신기에서 확인할 실제 통과 기준을 유지하고 중복 경고 생략')

    for row, (procedure, criterion) in CYBER_WORDS.items():
        put(CYBER, f'I{row}', procedure, '보안 시험의 실제 행동을 쉬운 문장으로 분리', False)
        put(CYBER, f'J{row}', criterion, '보안 통과 기준을 항목별로 구체화', False)

    # 측정 결과의 숫자/RTT/판정은 보존하며 잘못된 측정 명칭만 바로잡는다.
    for sheet in [MAIN, HELPER, '예상 그림']:
        for cell, old in source['sheets'][sheet]['cells'].items():
            before = current(sheet, cell)
            after = terminology(before)
            if after != before:
                put(sheet, cell, after, 'PBS 지그/파형 명칭 및 문장 구분자 정정')

    # 보조 시트는 식별 ID+조건 번호로 연결한다. 두 시트의 행 번호는 같지 않다.
    main_conditions = {}
    for test in tests:
        for row in range(test['row'], test['end'] + 1):
            text = current(MAIN, f'J{row}')
            match = re.search(r'조건\s*\[(\d+)\]', text)
            if match:
                main_conditions[(test['id'], int(match.group(1)))] = text.strip()
    hc = source['sheets'][HELPER]['cells']
    for addr, value in hc.items():
        if not re.fullmatch(r'A\d+', addr):
            continue
        match = re.fullmatch(r'SW-FW-SYS-Test-ID-(\d+)', value.strip())
        if not match:
            continue
        row = int(addr[1:])
        condition = re.search(r'조건\s*\[(\d+)\]', hc.get(f'B{row}', ''))
        if condition:
            key = (int(match.group(1)), int(condition.group(1)))
            assert key in main_conditions, key
            if hc[f'B{row}'].strip() != main_conditions[key]:
                put(HELPER, f'B{row}', main_conditions[key], '본문의 같은 시험 ID/조건과 입력 및 통과 기준을 동기화', False)

    sorted_changes = sorted(changes.values(), key=lambda x: (list(source['sheets']).index(x['sheet']), int(x['cell'][1:]), x['cell'][0]))
    assert all('·' not in x['after'] for x in sorted_changes)
    old_heights = json.loads((QA / 'plan.json').read_text('utf8')).get('heights', []) if (QA / 'plan.json').exists() else []
    plan_data = {'changes': sorted_changes, 'heights': old_heights, 'reviewed_ids': reviewed, 'issues': issues,
                 'map_evidence': {'generation': 'ci_map.c:543-555 / ci_map.h:21,25',
                                  'packet': 'stimulationParaCal.c:617-641', 'strategy': 'stimulationStrategy.c:stimulationStrategy_NofM'},
                 'scope': 'Plain wording / PBS solution and two jig paths / map3 NofM. Preserve test IDs, numeric results, verdicts and source code.'}
    (QA / 'plan.json').write_text(json.dumps(plan_data, ensure_ascii=False, indent=2), 'utf8')
    print(f'Plan: {len(sorted_changes)} cells / 81 purpose descriptions / 35 security rows.')


def expected_changes(kind, plan_data):
    if kind == 'system':
        return plan_data['changes']
    return [x for x in plan_data['changes'] if x['sheet'] in [MAIN, CYBER] and x['cell'][0] not in 'KL']


def build():
    plan_data = json.loads((QA / 'plan.json').read_text('utf8'))
    authored = reader.extract(QA / 'system-authored.xlsx')
    for kind in ['system', 'criterion']:
        package = native.parts(QA / f'{kind}-before.xlsx')
        style_mapping = equipment_wrap_styles(package, MAIN if kind == 'system' else CRITERION)
        changes = expected_changes(kind, plan_data)
        names = {x['sheet'] for x in changes} | {x['sheet'] for x in plan_data['heights'] if kind == 'system' or x['sheet'] in [MAIN, CYBER]}
        for source_name in names:
            name = CRITERION if kind == 'criterion' and source_name == MAIN else source_name
            part = native.sheet_path(package, name)
            root = native.parse(package[part])
            cells = {x.get('r'): x for x in root.findall('.//s:c', native.N)}
            for change in [x for x in changes if x['sheet'] == source_name]:
                value = authored['sheets'][source_name]['cells'][change['cell']]
                assert value == change['after']
                bridge.inline(cells[change['cell']], value)
                if source_name == MAIN and change['cell'] in style_mapping:
                    cells[change['cell']].set('s', str(style_mapping[change['cell']][1]))
            for item in [x for x in plan_data['heights'] if x['sheet'] == source_name]:
                row = root.find(f's:sheetData/s:row[@r="{item["row"]}"]', native.N)
                assert item['height'] <= 409.5
                row.set('ht', str(item['height']))
                row.set('customHeight', '1')
            package[part] = native.serialize(root)
        bridge.save(package, QA / f'{kind}-candidate.xlsx')
    print('Built original-format candidates from Artifact-authored text.')


def fit():
    """실제 Excel의 글꼴/열 폭으로 잰 높이를 해당 병합 영역에 확보한다."""
    data = json.loads((QA / 'plan.json').read_text('utf8'))
    heights = {(x['sheet'], x['row']): x['height'] for x in data['heights']}
    for kind in ['system', 'criterion']:
        check = json.loads((QA / f'{kind}-native-check.json').read_text('utf-8-sig'))
        for item in check['measurements']:
            name = MAIN if item['sheet'] == CRITERION else item['sheet']
            if item['needed'] <= item['current'] + 1:
                continue
            row = int(re.search(r'\d+', item['cell']).group())
            needed = item['needed'] - item['current'] + item['rowHeight'] + 3
            assert needed <= 409.5, ('Row needs a structural split; do not silently crop.', kind, name, row, needed)
            heights[(name, row)] = max(heights.get((name, row), 0), needed)
    data['heights'] = [{'sheet': name, 'row': row, 'height': height} for (name, row), height in sorted(heights.items())]
    (QA / 'plan.json').write_text(json.dumps(data, ensure_ascii=False, indent=2), 'utf8')
    print(f'Adjusted rows: {len(data["heights"])}')


def numeric_tokens(value):
    return re.findall(r'0[xX][0-9A-Fa-f]+|[0-9A-F]{8}|-?\d+(?:\.\d+)?', value)


def audit():
    """전체 통과 기준의 구체적 값/배열이 빠졌는지 별도 점검 자료를 만든다."""
    plan_data = json.loads((QA / 'plan.json').read_text('utf8'))
    source = reader.extract(QA / 'system-before.xlsx')
    array_re = re.compile(r'\[(?:0x[0-9A-Fa-f]+|-?\d+)(?:\s*,\s*(?:0x[0-9A-Fa-f]+|-?\d+))+\]')
    removed_arrays = []
    number_differences = []
    for change in plan_data['changes']:
        name, address, after = change['sheet'], change['cell'], change['after']
        if name not in [MAIN, CYBER] or not address.startswith('J'):
            continue
        before = source['sheets'][name]['cells'].get(address, '')
        normal = lambda v: re.sub(r'\s+', '', v)
        after_arrays = {normal(v) for v in array_re.findall(after)}
        missing = {normal(v) for v in array_re.findall(before)} - after_arrays
        if missing:
            removed_arrays.append({'sheet': name, 'cell': address, 'arrays': sorted(missing)})
        # 숫자의 쉼표/소수 표현은 정규화하되 식별자/상태 값은 실제 값으로 비교한다.
        def numbers(text):
            text = re.sub(r'(?<=\d),(?=\d{3}(?!\d))', '', text)
            vals = numeric_tokens(text)
            return {str(float(x)) if re.fullmatch(r'-?\d+(?:\.\d+)?', x) else x.upper() for x in vals}
        missing_numbers = numbers(before) - numbers(after)
        if missing_numbers:
            number_differences.append({'sheet': name, 'cell': address, 'values': sorted(missing_numbers)})
    output = {'missingArrays': removed_arrays, 'numericReview': number_differences}
    (QA / 'content-audit.json').write_text(json.dumps(output, ensure_ascii=False, indent=2), 'utf8')
    assert not removed_arrays, removed_arrays
    print(json.dumps(output, ensure_ascii=False))


def verify(published=False):
    data = json.loads((QA / 'plan.json').read_text('utf8'))
    baseline = json.loads((QA / 'baseline.json').read_text('utf8'))
    outputs = {}
    for kind, record in baseline['files'].items():
        path = Path(record['path']) if published else QA / f'{kind}-candidate.xlsx'
        old, new = reader.extract(QA / f'{kind}-before.xlsx'), reader.extract(path)
        mapping = {(CRITERION if kind == 'criterion' and x['sheet'] == MAIN else x['sheet'], x['cell']): x['after'] for x in expected_changes(kind, data)}
        height_map = {(CRITERION if kind == 'criterion' and x['sheet'] == MAIN else x['sheet'], x['row']): x['height'] for x in data['heights'] if kind == 'system' or x['sheet'] in [MAIN, CYBER]}
        assert list(old['sheets']) == list(new['sheets'])
        actual = []
        for name, source in old['sheets'].items():
            target = new['sheets'][name]
            assert source['dimension'] == target['dimension'] and source['merges'] == target['merges']
            for address in source['cells'].keys() | target['cells'].keys():
                left, right = source['cells'].get(address, ''), target['cells'].get(address, '')
                if left != right:
                    assert (name, address) in mapping, ('Unexpected change', kind, name, address)
                    actual.append((name, address))
                if (name, address) in mapping:
                    assert right == mapping[(name, address)]
                if kind == 'system' and name in [MAIN, CYBER] and re.fullmatch(r'[KL]\d+', address):
                    assert numeric_tokens(left) == numeric_tokens(right), ('Numeric result changed', name, address)
                    if address.startswith('L'):
                        assert left == right, ('Verdict changed', name, address)
        assert len(actual) == len(mapping)
        before_package, after_package = native.parts(QA / f'{kind}-before.xlsx'), native.parts(path)
        allowed_package = dict(before_package)
        equipment_sheet = MAIN if kind == 'system' else CRITERION
        style_mapping = equipment_wrap_styles(allowed_package, equipment_sheet)
        assert allowed_package['xl/styles.xml'] == after_package['xl/styles.xml'], 'Unexpected style edit'
        modified = {native.sheet_path(after_package, n) for n, _ in mapping.keys() | height_map.keys()}
        assert before_package.keys() == after_package.keys()
        for part, original in before_package.items():
            if part == 'xl/styles.xml':
                continue
            if part not in modified:
                assert original == after_package[part], ('Unrelated package change', kind, part)
                continue
            left, right = native.parse(original), native.parse(after_package[part])
            for (name, address) in mapping:
                if native.sheet_path(after_package, name) != part:
                    continue
                a = left.find(f'.//s:c[@r="{address}"]', native.N)
                b = right.find(f'.//s:c[@r="{address}"]', native.N)
                if name == equipment_sheet and address in style_mapping:
                    assert b.get('s') == str(style_mapping[address][1])
                    b.set('s', a.get('s'))
                assert a.get('s') == b.get('s') and a.get('r') == b.get('r')
                for c in [a, b]:
                    for node in list(c):
                        c.remove(node)
                    c.attrib.pop('t', None)
            for (name, row), height in height_map.items():
                if native.sheet_path(after_package, name) != part:
                    continue
                for root in [left, right]:
                    item = root.find(f's:sheetData/s:row[@r="{row}"]', native.N)
                    if root is right:
                        assert abs(float(item.get('ht')) - height) < 0.01
                    item.attrib.pop('ht', None)
                    item.attrib.pop('customHeight', None)
            assert native.serialize(left) == native.serialize(right), ('Structure/style changed', kind, part)
        name = MAIN if kind == 'system' else CRITERION
        cells = new['sheets'][name]['cells']
        ids = [v.strip() for a, v in cells.items() if re.fullmatch(r'B\d+', a) and re.fullmatch(r'SW-FW-SYS-Test-ID-\d+', v.strip())]
        assert len(ids) == len(set(ids)) == 81
        old_conditions = [(a, re.findall(r'조건\s*\[\d+\]', v)) for a, v in old['sheets'][name]['cells'].items() if re.fullmatch(r'J\d+', a)]
        new_conditions = [(a, re.findall(r'조건\s*\[\d+\]', v)) for a, v in cells.items() if re.fullmatch(r'J\d+', a)]
        assert old_conditions == new_conditions
        assert all('PBS 지그' not in v and 'PBS 저항' not in v for s in new['sheets'].values() for v in s['cells'].values())
        outputs[kind] = {'changedCells': len(actual), 'ids': len(ids), 'sha256': reader.sha(path)}
    report_path = Path(baseline['files']['system']['path']) if published else QA / 'system-candidate.xlsx'
    criterion_path = Path(baseline['files']['criterion']['path']) if published else QA / 'criterion-candidate.xlsx'
    r, c = reader.extract(report_path), reader.extract(criterion_path)
    for report_name, criterion_name in [(MAIN, CRITERION), (CYBER, CYBER)]:
        rc, cc = r['sheets'][report_name]['cells'], c['sheets'][criterion_name]['cells']
        for address in rc.keys() | cc.keys():
            if re.fullmatch(r'[A-J]\d+', address) and address != 'B2':
                assert rc.get(address, '') == cc.get(address, ''), ('Common column mismatch', report_name, address)
    for filename, digest in baseline['protected'].items():
        assert reader.sha(Path(filename)) == digest, ('Protected source changed', filename)
    result = {'files': outputs, 'protectedFiles': len(baseline['protected']), 'commonColumnsEqual': True,
              'numericResultsAndVerdictsPreserved': True, 'newBoardTests': False, 'map3NofMCodeChecked': True}
    (QA / ('published-verification.json' if published else 'verification.json')).write_text(json.dumps(result, ensure_ascii=False, indent=2), 'utf8')
    print(json.dumps(result, ensure_ascii=False))


def publish():
    verify()
    baseline = json.loads((QA / 'baseline.json').read_text('utf8'))
    for kind in baseline['files']:
        check = json.loads((QA / f'{kind}-native-check.json').read_text('utf-8-sig'))
        assert check['readOnly'] and check['hash'] == reader.sha(QA / f'{kind}-candidate.xlsx')
        assert all(x['needed'] <= x['current'] + 1 for x in check['measurements'])
    for record in baseline['files'].values():
        assert reader.sha(Path(record['path'])) == record['sha256'], 'Target was edited externally; preserve it.'
        # 하나를 저장한 뒤 다른 파일의 잠금을 발견하지 않도록 두 대상의 쓰기 접근을 먼저 확인한다.
        with Path(record['path']).open('r+b'):
            pass
    for kind, record in baseline['files'].items():
        shutil.copyfile(QA / f'{kind}-candidate.xlsx', record['path'])
    verify(published=True)


if __name__ == '__main__':
    {'prepare': prepare, 'plan': plan, 'build': build, 'fit': fit, 'audit': audit, 'verify': verify, 'publish': publish}[sys.argv[1]]()
