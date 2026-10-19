"""BOOT 입출력 문서의 독립 기대값과 저장된 관찰을 대조한다.

파일을 직접 쓰지 않는다. --patch는 지정 산출물용 apply_patch 입력을 출력하고,
--check는 저장된 문서/JSON과 소스의 벡터 대응을 읽기 전용으로 검사한다.
제품/시험 실행 및 baseline 검증은 수행하지 않는다.
"""
from __future__ import annotations

import argparse
import difflib
import json
import os
from pathlib import Path
import re
import struct
import sys
from urllib.parse import quote, unquote

ROOT = Path(__file__).resolve().parents[5]
TASK = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/참고/인허가/사운드1/DHF/04_설계 검증/SW/펌웨어/유닛 시험 예상 입출력/BOOT'
INDEX = TASK / 'qa/expected-io/boot.json'
QA = TASK / 'qa/source-preserving-tests/boot/checks.json'
RUNNER = 'src/0__bootloader/source/test/tdc_ut_boot.c'
HOST = 'tests/unit/tdc_ut_boot_host.c'
PRODUCT = 'src/0__bootloader/source/bootloader.c'
COVERAGE = 'tests/unit/boot_coverage.json'
CASES: dict[int, list[dict]] = {n: [] for n in range(1, 21)}
INTRO: dict[int, str] = {}
LIMITS: dict[int, list[str]] = {}
SOURCES: dict[int, list[str]] = {}


def crc16(data: bytes) -> int:
    """시험의 독립 CCITT 식: init FFFF, poly 1021, 최종 XOR/반사 없음."""
    crc = 0xffff
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xffff
    return crc


def signed(value: int) -> int:
    return value - 0x100000000 if value > 0x7fffffff else value


def add(unit: int, case: int, title: str, procedure: str,
        fields: str, expected: list[int], function: str, *, ram: bool = False,
        behavior: str = '', characterization: bool = False) -> None:
    names = fields.split(';')
    assert len(names) == len(expected), (unit, case, fields, expected)
    CASES[unit].append(dict(key=f'C{case}', number=case, title=title,
        procedure=procedure, fields=names, expected=[signed(x) for x in expected],
        source=f'{RUNNER if ram else HOST}:{function}', ram=ram,
        behavior=behavior, characterization=characterization))


def ret(unit: int, case: int, title: str, procedure: str, expected: int,
        function: str, *, ram: bool = False, behavior: str = '') -> None:
    add(unit, case, title, procedure, '함수 반환값 (정수)', [expected], function,
        ram=ram, behavior=behavior)


def define_cases() -> None:
    # 각 expected는 아래에서 C 시험의 상수/독립 판정식으로 작성한다.
    # QA.actual은 render에서 관찰 열에만 사용하며 기대값 생성에 사용하지 않는다.
    for case, value in [(1, 0), (2, 1000), (3, 0), (4, 1), (5, 999), (6, 1000)]:
        add(2, case, f'{value} us 요청', f'SystemCoreClock=30,720,000 Hz로 설정하고 tdc_delay_us({value})를 호출한다.',
            'Sys_Delay에 전달한 cycle 수 (cycle)', [(30720000 // 1000000) * value], 'run_delay',
            characterization=True)
    for case, value in zip(range(7, 11), [0, 1, 999, 1000]):
        add(2, case, f'{value} ms 요청', f'30,720,000 Hz에서 tdc_delay_ms({value})를 호출한다.',
            'Sys_Delay에 전달한 cycle 수 (cycle)', [(30720000 // 1000) * value], 'run_delay', characterization=True)
    for case, value, name, divisor in [(11, 1000, 'us', 1000000), (12, 1, 'ms', 1000)]:
        add(2, case, f'32 MHz에서 {value} {name}', f'기존 클럭을 보관하고 SystemCoreClock=32,000,000 Hz로 바꿔 tdc_delay_{name}({value})를 호출한 뒤 클럭을 복원한다.',
            'Sys_Delay에 전달한 cycle 수 (cycle)', [32000000 // divisor * value], 'run_delay', characterization=True)
    add(4, 1, '초기화와 시작', 'tdc_boot_led_init → tdc_boot_led_start 순서로 호출한다. elapsed=0, PWM bin=0, ms 내부 카운터=0, G/B duty=0에서 시작한다.', '경과 카운터 (ms)', [0], 'run_led')
    add(4, 2, '100 ms 모의 진행', 'C1에 이어 timer_active=1(타이머 가동)일 때만 ISR을 40×100=4,000회 호출한다.', '경과 카운터 (ms)', [100], 'run_led')
    add(4, 3, '361 ms에서 완료', 'C2에 이어 ISR 40×261=10,440회를 추가한다. 누계는 14,440회다.', '완료 여부 (0=진행, 1=완료)', [1], 'run_led')
    add(4, 4, '정지 후 출력 유지', 'C3 후 stop을 호출하고 GPIO 쓰기 횟수를 보관한다. 100회 반복하되 timer_active=0(정지)이므로 ISR은 호출하지 않는다.',
        '경과 카운터 (ms);완료 여부 (0=진행, 1=완료);정지 직후 대비 GPIO 쓰기 횟수 불변 (0=다름, 1=같음)', [361, 1, 1], 'run_led')
    add(4, 5, '1 ms 관찰 횟수', 'C1~C4의 단일 실행에서 LED observer 호출 수를 읽는다.', 'LED 관찰 콜백 횟수 (회)', [361], 'run_led')
    for c, title, step, exp in [
        (1, 'NVM 초기화 실패', '공개 NVM 자료를 준비하고 NVMInit 반환=-7을 주입한 뒤 tdc_boot_storage_init(NULL)을 호출한다.', -7),
        (3, '마운트 실패', '경계 상태를 초기화하고 f_mount 반환=FR_DISK_ERR(1)로 호출한다.', 1),
        (4, '정상 저장장치 준비', '오류를 모두 0(미주입)으로 초기화하고 tdc_boot_storage_init(NULL)을 호출한다.', 0),
        (6, '제조 표 CRC 오류 전파', '공개 NVM의 바이트[1024]를 XOR 1로 손상시키고 저장장치 준비를 호출한다. CRC는 다시 만들지 않는다.', -1)]:
        ret(7, c, title, step, exp, 'run_storage')
    add(7, 2, '초기화 실패 시 조기 종료', 'C1 직후 경계 이벤트 수를 읽는다. NVMInit만 기록되고 mount까지 진행하지 않아야 한다.', '경계 이벤트 횟수 (회)', [1], 'run_storage')
    add(7, 5, 'NVM 다음에 mount', 'C4 직후 이벤트 배열의 인덱스 0과 1을 확인한다.', 'event[0] (1=NVMInit);event[1] (2=f_mount)', [1, 2], 'run_storage')
    storage = [
        ('첫 기동정보 읽기 실패', 'speed=1, fail_mask=1: 첫 NVMReadBytes만 -9', -9, 0),
        ('예약 속도값 거부', 'speed=255, fail_mask=0', -1, 0),
        ('드라이브 선택 실패', 'speed=1, f_chdrive=1(FR_DISK_ERR)', 1, 0),
        ('디렉터리 열기 실패', 'speed=1, f_opendir=1(FR_DISK_ERR)', 1, 0),
        ('디렉터리 닫기 실패', 'speed=1, f_closedir=1(FR_DISK_ERR)', 1, 0),
        ('NVM 동기화 실패', 'speed=1, sync_error=1: NVMSync=false(0)', -1, 0),
        ('AUTO의 QSPI 실패 후 DSPI', 'speed=0, fail_mask=2: 두 번째 NVM 읽기만 -9', 0, 2),
        ('AUTO의 QSPI/DSPI 실패 후 기본값', 'speed=0, fail_mask=6: 두 번째/세 번째 NVM 읽기를 -9로 설정', 0, 3),
        ('명시 DSPI 재읽기 실패', 'speed=2, fail_mask=2', -9, 1),
        ('명시 QSPI 재읽기 실패', 'speed=3, fail_mask=2', -9, 1),
        ('제조 표 읽기 실패 전파와 출력 보존', 'speed=1, fail_mask=2: 첫 기동정보 읽기는 성공하고 다음 제조 표 읽기는 -9', -9, 0),
        ('오류 후 정상 회복', 'speed=1, fail_mask=0, 모든 오류 주입=0', 0, 0)]
    for c, (title, setup, result, reinit) in enumerate(storage, 7):
        add(7, c, title, f'각 건마다 공개 NVM을 새로 준비하고 추적값/오류를 0으로 초기화한다. out의 바이트[0..31] 전부를 0xA6으로 채운다. {setup}. 변경된 기동정보의 CRC를 다시 계산한 뒤 tdc_boot_storage_init(&out)을 호출한다.',
            '반환값;출력 32바이트 일치 (0=불일치, 1=일치);NVMReInitOptions 호출 횟수 (회)', [result, 1, reinit], 'run_storage',
            behavior='반환 기대가 0이면 out은 입력 기동정보 32바이트와 같아야 한다. 실패 기대이면 32바이트 모두 0xA6을 유지해야 한다. 모든 C7~C18에서 모의 코어 기동 횟수는 0이어야 한다.')
    ret(8, 1, 'PC 기동정보 정상 읽기', '공개 NVM과 0으로 채운 uint32_t data[128]를 준비하여 bootloader_read_nvm_boot_info(data)를 호출한다.', 0, 'run_boot_info')
    add(8, 2, 'PC 구조체 크기와 필드', 'C1의 결과를 packed 기동정보로 읽는다.', '구조체 크기 (byte);boot_addr (CFX 주소값);boot_manu_off (byte);boot_manu_len (32비트 word)', [32, 0x12345678, 1024, 64], 'run_boot_info')
    ret(8, 3, 'PC 기동정보 CRC 손상', 'NVM 바이트[512]를 XOR 1로 바꾸고 같은 읽기를 호출한다.', -1, 'run_boot_info')
    ret(8, 4, 'PC NULL 출력', 'bootloader_read_nvm_boot_info(NULL)을 호출한다.', -1, 'run_boot_info')
    ret(8, 5, 'PC 제조 표 적재', '공개 NVM을 복구하고 bootloader_load_manu_table(1024)을 호출한다. trim 함수는 PC 모의 경계다.', 0, 'run_boot_info')
    add(8, 6, 'PC 제조 예약 워드 전달', 'C5 후 모의 제조 표 Reserved[0]을 읽는다.', 'Reserved[0] (uint32 bit pattern)', [0x11223344], 'run_boot_info')
    add(8, 101, 'RAM 512바이트 읽기', 'RAM data[0..511]를 0으로 초기화하고 앞 32바이트에 기동정보를 놓는다. out[0..127]=0, reads=0에서 실제 읽기 함수를 호출한다.',
        '반환값;구조체 크기 (byte);boot_addr (CFX 주소값);boot_manu_off (byte);boot_manu_len (word);RAM 읽기 횟수 (회)', [0,32,0x12345678,1024,64,1], 'run_ram_boot_info', ram=True)
    for c, title, proc, exp in [
        (102,'RAM CRC 손상','C101 뒤 ram.data[0] ^= 1로 변경하고 재호출한다.',-1),
        (103,'RAM 읽기 오류','C102 뒤 error=-7을 주입하고 재호출한다.',-7),
        (104,'RAM NULL 출력','C103 뒤 NULL 출력을 전달한다.',-1)]:
        ret(8,c,title,proc,exp,'run_ram_boot_info',ram=True)
    add(8,105,'NULL은 읽지 않음','C101~C104 뒤 누적 reads를 읽는다.', 'RAM 읽기 횟수 (회)',[3],'run_ram_boot_info',ram=True)
    add(8,106,'RAM 오류 후 전체 복구','error=0으로 해제하고 ram.data[0]을 다시 XOR 1하여 복구한다. 읽기 함수를 호출하고 out 전체 512바이트를 RAM 입력과 비교한다.',
        '반환값;512바이트 전체 일치 (0=다름, 1=같음)',[0,1],'run_ram_boot_info',ram=True)
    add(9,1,'이름과 확장자 결합','기동정보 전체를 0으로 하고 boot_app_load[0..4]="test\\0", 나머지[5..9]=0, boot_app_ext[0..3]="app\\0"로 설정한다. name[0..19]=0에서 get_app_load_ext를 호출한다.',
        '길이 (문자, NUL 제외);name[0] ASCII;name[1] ASCII;name[2] ASCII;name[3] ASCII;name[4] ASCII;name[5] ASCII;name[6] ASCII;name[7] ASCII',
        [8,116,101,115,116,46,97,112,112],'run_name',ram=True,behavior='문자열은 test.app, name[8]=NUL이다. 자동 payload는 앞 8문자와 길이를 비교한다.')
    for c,title,proc in [(2,'빈 이름','C1 뒤 boot_app_load[0]=0으로 한다.'),(3,'빈 확장자','이름을 test로 복구하고 boot_app_ext[0]=0으로 한다.'),(4,'NULL 정보','정보 포인터=NULL, name은 유효 버퍼로 전달한다.'),(5,'NULL 결과','정보는 C3 상태의 유효 포인터이고 출력 포인터=NULL이다.')]:
        ret(9,c,title,proc+' get_app_load_ext를 호출한다.',-1,'run_name',ram=True)
    file_cases=[('파일 없음','missing.FEZ가 없는 상태'),('짧은 파일','정상 무섹션 헤더의 첫 3바이트만 short.FEZ에 저장'),('CRC 오류 파일','정상 헤더 byte[7] XOR 1을 bad.FEZ에 8바이트 저장'),('정상 무섹션 파일','정상 무섹션 헤더 8바이트를 A.FEZ에 저장')]
    for c,(title,proc) in enumerate(file_cases,1):
        name=['missing.FEZ','short.FEZ','bad.FEZ','A.FEZ'][c-1]
        ret(10,c,title,f'{proc}에서 bootloader_load_app_file("{name}")를 호출한다.',0 if c==4 else -1,'run_file')
    add(10,5,'무섹션 파일은 전송 없음','C1~C4 뒤 전송 경계 누계를 읽는다.','모의 데이터 전송 횟수 (회)',[0],'run_file')
    add(10,6,'무섹션 파일은 코어 기동 없음','C1~C4 뒤 코어 기동 경계 누계를 읽는다.','모의 코어 기동 횟수 (회)',[0],'run_file')
    ret(10,7,'NULL 파일명','bootloader_load_app_file(NULL)을 호출한다.',-1,'run_file')
    ret(10,8,'255개 CRC 목록 용량 초과','정상 헤더의 crc_table_size만 255로 바꾼 첫 8바이트를 large.FEZ에 저장한다. load_app_file의 출력 버퍼는 512바이트다.',-1,'run_file')
    ret(10,9,'지원하지 않는 식별자','ident=0, crc_table_size=0인 8바이트 헤더에 정상 CRC를 붙여 unknown.FEZ를 적재한다.',-1,'run_file')
    hs=[(101,'정상 8바이트 헤더','정상 무섹션 평문 헤더, 파일 위치=0, 출력 용량=32바이트',0),
        (102,'3바이트 헤더','파일 길이=3, 위치=0',-1),(103,'첫 읽기 실패','길이=8, 위치=0, fail=1로 FR_DISK_ERR(1) 반환',-1),
        (104,'식별자 비트 손상','fail=0으로 해제, 평문 헤더 word[0] XOR 1, CRC 재계산 없음',-1),
        (106,'암호화 블록 1바이트 부족','암호화 헤더의 첫 19바이트만 제공, 위치=0',-1),(107,'암호화 읽기 실패','암호화 헤더 길이=20, 위치=0, fail=1',-1),
        (108,'CRC 목록 255개','fail=0, 위치=0, 평문 ident 복구, word[1]=255, 입력 길이=8, 출력 용량=32',-1),
        (111,'두 번째 읽기 1바이트 부족','정상 12바이트 헤더의 앞 11바이트만 제공, 위치=0, 출력 용량=32',-1),
        (112,'두 번째 읽기 오류','정상 12바이트 입력, 위치/읽기 수=0, fail_on_read=2',-1),
        (113,'헤더 읽기 회복','정상 12바이트 입력, 위치=0, fail_on_read=0(미주입)',0)]
    for c,title,proc,exp in hs:
        ret(10,c,title,proc+'에서 bootloader_read_and_check_file_header를 호출한다.',exp,'run_ram_header',ram=True)
    add(10,105,'암호화 헤더 해독','공개 키로 생성한 20바이트 암호화 헤더, 위치=0, 출력 용량=32에서 헤더 파서를 호출한다.', '반환값;ident (uint32);CRC 목록 개수 (개)',[0,0x455a4149,0],'run_ram_header',ram=True)
    add(10,109,'12바이트 정확한 용량','목록 1개와 padding을 포함한 정상 12바이트 헤더를 제공하고 출력 용량도 12로 호출한다.', '반환값;CRC 목록 개수 (개);소비한 입력 길이 (byte)',[0,1,12],'run_ram_header',ram=True)
    add(10,110,'11바이트 출력 한계와 보호값','output[0..31]를 0xA5로 채우고 정상 12바이트 입력, 위치=0에서 출력 용량=11로 파서를 호출한다.', '반환값;출력 byte[11] 보호값',[ -1,0xa5],'run_ram_header',ram=True)
    for c,title,proc,exp in [(1,'정상 CFX P 구획','data word[0..3]={0x11,0x22,0x33,0x44}, type=3, size=4, offset=0, flags=0, 정상 data CRC',0),
        (2,'미지원 구획 종류','같은 파일, type=0xFFFF, data_crc=0',-1),
        (3,'PRAM 끝 다음 주소','type=3, offset=(0x2023FFFF-0x20220000+1)/4=32768 word, size=4, data_crc=0',-1),
        (4,'AES 3워드 길이 거부','offset=0, type=3, flags=0x80(암호화), size=3 word, data_crc=0',-1),
        (5,'파일보다 큰 5워드 요청','flags=0, type=3, size=5 word이나 파일은 16바이트, data_crc=0',-1)]:
        ret(11,c,title,proc+'. 파일을 처음부터 열어 bootloader_transfer_section_data를 호출하고 닫는다.',exp,'run_sections')
    ret(11,6,'선언한 한 구획 누락','0으로 초기화한 app_header에 crc_table_size=1을 설정한다. 빈 파일과 0으로 초기화한 CFX/CM3 출력으로 load_sections를 호출한다.',-1,'run_sections')
    add(11,101,'CM3 기동 자료 추출','type=9, size=2 word, flags=0의 헤더와 평문 word[0..1]={0x20007FF0,0x00012345}, 정상 data CRC를 주고 bootloader_load_boot_section을 호출한다.',
        '반환값;CM3 stack_pointer (byte 주소);CM3 init_pointer (byte 주소)',[0,0x20007ff0,0x12345],'run_ram_sections',ram=True)
    for c,title,proc in [(102,'기동 자료 3바이트','C101의 파일 위치=0, 길이=3으로 재호출'),(103,'기동 자료 CRC 오류','위치=0, 길이=8, data_crc XOR 1로 재호출'),(104,'NULL 기동 헤더','section_header=NULL로 호출')]:
        ret(11,c,title,proc+'. 이전 출력의 구체적 값은 이 payload에서 비교하지 않는다.',-1,'run_ram_sections',ram=True)
    add(11,105,'CFX 다음 CM3의 두 구획','48바이트의 두 기동 구획을 제공한다. CRC 목록 2개, CFX/CM3 출력은 0으로 초기화한 후 load_sections를 호출한다.',
        '반환 기동자료 플래그 (bit0=CFX, bit1=CM3);CFX stack_pointer;CFX init_pointer (CFX 주소);CM3 stack_pointer (byte 주소);CM3 init_pointer (byte 주소)',[3,0,0x23456,0x20007ff0,0x12345],'run_ram_sections',ram=True,
        behavior='3은 두 기동 자료를 파싱했다는 뜻이다. 실제 이미지 전송 또는 코어 실행 완료 플래그가 아니다.')
    for c,title,proc in [(106,'첫 구획 헤더 CRC 손상','첫 헤더 word[0] XOR 1, 파일 위치=0'),(107,'첫 구획 데이터 부족','첫 헤더를 복구하고 입력 길이를 16+3=19바이트로 제한'),(108,'선언한 두 구획 누락','CRC 목록 개수=2를 유지하고 입력 길이=0'),(109,'첫 헤더 읽기 실패','정상 48바이트 입력으로 복구, 읽기 수=0, fail_on_read=1'),(110,'CFX 뒤 CM3 헤더 읽기 실패','정상 48바이트 입력, 읽기 수=0, fail_on_read=3')]:
        ret(11,c,title,proc+', 위치=0에서 load_sections를 호출한다.',-1,'run_ram_sections',ram=True)
    add(11,111,'NULL 기동 출력은 읽지 않음','위치/읽기 수=0, fail_on_read=0에서 load_boot_section의 boot_data=NULL로 호출한다.', '반환값;RAM 읽기 횟수 (회)',[-1,0],'run_ram_sections',ram=True)
    add(11,112,'암호화 기동 자료','평문 word[0..3]={0x20007FF0,0x00012345,0x11223344,0x55667788}를 공개 키로 암호화한 16바이트를 제공한다. type=9, size=2, flags=0x80, 평문 전체 16바이트의 data CRC로 load_boot_section을 호출한다.',
        '반환값;CM3 stack_pointer (byte 주소);CM3 init_pointer (byte 주소)',[0,0x20007ff0,0x12345],'run_ram_sections',ram=True)
    for c,title,proc in [(113,'AES 15바이트 짧은 입력','16바이트 암호문의 첫 15바이트만 제공'),(114,'AES 데이터 CRC 오류','암호문 16바이트를 제공하되 data_crc XOR 1')]:
        add(11,c,title,'출력 stack_pointer=0x00654321, init_pointer=0x00123456으로 놓고 위치=0에서 '+proc+'하여 load_boot_section을 호출한다.',
            '반환값;출력 8바이트 보존 (0=다름, 1=같음)',[-1,1],'run_ram_sections',ram=True)
    for c,title,proc,exp in [(1,'두 파일 목록','A.FEZ와 B.FEZ는 각각 정상 무섹션 헤더 8바이트, 목록은 A.FEZ\\nB.FEZ\\n의 12바이트',0),
        (3,'목록 첫 읽기 오류','정상 목록을 다시 열고 s_tdc_read_error=1로 모든 파일 읽기에 FR_DISK_ERR(1) 주입',-1),
        (4,'목록의 대상 파일 없음','목록을 missing.FEZ\\n의 12바이트로 바꾸고 CRC 재계산',-1),
        (5,'목록 CRC 불일치','C4 목록과 파일을 유지하고 CRC 목록[0] XOR 1',-1),
        (6,'NULL 목록 헤더','file_desc=NULL, app_header=NULL',-1)]:
        ret(12,c,title,proc+'에서 bootloader_process_manifest를 호출한다. 오류 후 남은 열린 목록은 호출자가 닫는다.',exp,'run_manifest')
    add(12,2,'PC 대상 파일 순서','C1 직후 연 파일 횟수와 순서를 확인한다.', '연 app 파일 수 (개);첫 파일 ID (1=A.FEZ);둘째 파일 ID (2=B.FEZ)',[2,1,2],'run_manifest')
    for c,offset,ch in [(101,0,'A'),(102,6,'B')]:
        add(12,c,f'{ch}.FEZ 항목 분리',f'목록 byte[{offset}]를 시작으로 get_manifest_files(src,name,16)을 호출한다. name은 내부에서 16바이트 모두 0으로 초기화된다.',
            '복사한 길이 (문자);name[0] ASCII;name[5] NUL',[5,ord(ch),0],'run_ram_manifest',ram=True,
            behavior=f'name[0..5]="{ch}.FEZ\\0", name[6..15]=0. payload는 길이/첫 글자/NUL만 비교한다.')
    for c,title,proc,exp in [(103,'NULL 목록','src=NULL, name은 유효, len=16',-1),(104,'NULL 이름 출력','src는 유효 목록, dst=NULL, len=16',-1),(105,'0바이트 출력 용량','src/dst는 유효, len=0',0)]:
        ret(12,c,title,proc+'으로 get_manifest_files를 호출한다.',exp,'run_ram_manifest',ram=True)
    add(12,106,'RAM 목록을 한 번씩 순서대로 처리','RAM 목록 A.FEZ\\nB.FEZ\\n, 정상 CRC, ident=0x657A616D, crc_table_size=1을 준비한다. open/read/close/chdir를 RAM 콜백으로 연결하고 process_manifest를 호출한다. 두 대상은 무섹션 app이다.',
        '반환값;대상 open 성공 수;order[0] (1=A);order[1] (2=B);두 handle 닫힘 (0=열림,1=닫힘);닫힌 목록 재읽기 횟수 (회)',[0,2,1,2,1,1],'run_ram_manifest',ram=True,
        behavior='마지막 값 1은 현재 구현이 닫은 목록을 한 번 다시 읽는 특성이다. 다중 블록 처리나 파일 수명 적합성의 PASS 근거가 아니다.',characterization=True)
    for c,title,proc in [(107,'RAM 목록 첫 읽기 실패','read_error=1, FR_DISK_ERR(1) 반환'),(108,'RAM B 파일 없음','missing_b=1, B.FEZ open에서 FR_NO_FILE(4)'),(109,'RAM 목록 CRC 오류','정상 목록 CRC 항목 XOR 1'),(110,'중첩 매니페스트 거부','nested=1, 열린 대상 헤더 ident=0x657A616D이고 CRC 정상'),(111,'RAM 슬롯 경로 실패','chdir_error=1로 FR_NO_PATH(5) 반환')]:
        add(12,c,title,'C106과 같은 새 RAM 상태로 시작하되 '+proc+'을 주입한다. process_manifest 호출 후 app handle을 검사하고 남은 manifest handle은 정리한다.',
            '반환값;app handle 닫힘 (0=열림,1=닫힘)',[-1,1],'run_ram_manifest',ram=True)
    crc_inputs=[('평문 헤더 CRC 정상','offset=0, size=4, type=3, flags=0, aes_pad_count=0, reserved=0 헤더에 독립 CRC를 붙인다.',0),
        ('NULL 헤더','헤더 포인터=NULL을 전달한다.',-1),
        ('암호화 헤더 CRC 정상','flags=0x80으로 새 CRC를 붙인 16바이트 헤더 전체를 공개 키로 암호화한다.',0),
        ('복호화 후 암호화 표시 없음','직전 호출이 복호화한 헤더에 flags=0을 설정하고 CRC를 재계산한 뒤 다시 암호화한다.',-1),
        ('복호화 후 CRC 손상','flags=0x80으로 CRC를 새로 만든 뒤 offset XOR 1로 손상시키고 암호화한다.',-1)]
    for ram,base in [(False,0),(True,100)]:
        for i,(title,proc,exp) in enumerate(crc_inputs,1):
            ret(13,base+i,title,proc+' bootloader_check_header_crc를 호출한다.',exp,'run_ram_crc' if ram else 'run_crc',ram=ram)
        for i,length in enumerate([0,1,3,4,5]):
            value=crc16(bytes([1,2,3,4,5])[:length])
            add(13,base+6+i,f'CRC 바이트 길이 {length}',f'uint32_t words[0..1]={{0x04030201,0x00000005}}를 입력하고 길이={length} byte로 bootloader_CRC_calc를 호출한다.',
                'CCITT CRC (16비트 정수)',[value],'run_ram_crc' if ram else 'run_crc',ram=ram)
            if not ram:
                add(13,11+i,f'CRC 경계에 전달한 {length}바이트',f'C{6+i} 직후 Sys_CRC_Add에 전달된 바이트 누계를 확인한다.', 'CRC 하드웨어 모형 입력 길이 (byte)',[length],'run_crc')
    add(14,1,'공개 AES 128 ECB 한 블록','문맥을 공개 key[0..3]으로 초기화하고 ciphertext[0..3]에 bootloader_decrypt_data(ctx,data,4)를 호출한다. 4는 바이트가 아닌 uint32 워드 수다.',
        '문맥 초기화 반환값;복호화 반환값;data[0] (uint32 bit pattern);data[1] (uint32 bit pattern);data[2] (uint32 bit pattern);data[3] (uint32 bit pattern)',
        [0,0,0x6bc1bee2,0x2e409f96,0xe93d7e11,0x7393172a],'run_aes',ram=True)
    add(14,2,'3워드 거부와 원본 보존','C1 이전에 만들어 둔 원래 암호문 4워드 사본에 len=3을 적용한다. C1의 복호화된 데이터를 재사용하지 않는다.',
        '복호화 반환값;암호문 16바이트 보존 (0=다름,1=같음)',[-1,1],'run_aes',ram=True)
    ret(14,3,'NULL AES 문맥','bootloader_initialize_aes_ctx(NULL,key)을 호출한다.',-1,'run_aes',ram=True)
    ret(14,4,'NULL AES 키','bootloader_initialize_aes_ctx(&ctx,NULL)을 호출한다.',-1,'run_aes',ram=True)
    for i,(kind,dest) in enumerate([(1,0x20800000),(2,0x20c50000),(3,0x20220000),(4,0x10000),(5,0x20008000)],1):
        mode=0x58000 if i<3 else 0
        length=16 if i<3 else 4
        proc=f'매 건 추적값을 초기화한다. type={kind}, size=4 word, flags=0, aes_pad_count=0, offset='+('0x08002000 word (DRAM_BASE/4)' if i==5 else '0 word')+'. data.bin의 16바이트 자료와 정상 data CRC로 transfer_section_data를 호출하고 파일을 닫는다.'
        add(15,i,f'구획 type={kind} 전송',proc,'반환값;대상 주소 (byte);복사 자료 길이 (byte);DMA count;DMA 워드 모드;자료 전체 16바이트 일치 (0=다름,1=같음);전송 횟수 (회)',
            [0,dest,16,length,mode,1,1],'run_transfer')
        add(15,100+i,f'구획 type={kind} 관찰 콜백',f'C{i} 직후 같은 전송의 observer 값[0..3]을 읽는다. 별도 전송을 수행하지 않는다.',
            '구획 type;대상 주소 (byte);DMA 워드 모드;DMA count',[kind,dest,mode,length],'run_transfer')
    ret(15,6,'EMPTY 구획의 0 채움','추적값을 초기화하고 type=4, size=4 word, offset=0, flags=0x10, data_crc=0, 열지 않은 0 초기화 FIL을 전달한다.',0,'run_transfer')
    add(15,7,'0 채움 전체 비교','C6 뒤 수집한 전송 byte[0..15]를 각각 0과 비교한다.', '16바이트 전체 0 여부 (0=다름,1=모두 0)',[1],'run_transfer')
    ret(16,1,'CFX PRAM 잠금 구획','lock.bin에 표준 16바이트 자료를 놓고 type=3, size=4, offset=0, flags=0x20, 정상 CRC로 transfer_section_data를 호출한다.',0,'run_lock')
    add(16,2,'잠금 해제 다음 전송 다음 잠금','C1의 추적값을 읽는다.', '잠금 설정 횟수 (회);locks[0] (0=CFX PRAM0/1 잠금 해제);locks[1] (1=CFX PRAM0 잠금);전송 이후 잠금 설정 횟수 (회)',[2,0,1,1],'run_lock')
    add(17,1,'기본 dump 명령','공개 NVM을 준비하고 4개 파일 각각에 12 34 두 바이트를 저장한다. 서비스 표시 상태를 보관한 뒤 RTT 입력 큐에 dump\\n을 넣고 service_main을 호출한다.',
        'MANIFEST의 정확한 파일 블록 포함 (0=없음,1=있음);FW DUMP END 포함 (0=없음,1=있음);모의 코어 기동 횟수 (회)',[1,1,0],'run_service')
    add(17,2,'알 수 없는 명령','경계 추적을 초기화하고 unknown\\n을 입력한다.', 'invalid command. 포함 (0=없음,1=있음)',[1],'run_service')
    add(17,3,'첫 파일 읽기 오류','s_tdc_read_error=1로 파일 read만 실패시키고 dump\\n을 입력한다. NVM 초기화는 성공한다.', '[ERROR] f_read 포함 (0=없음,1=있음)',[1],'run_service')
    add(17,4,'마지막 파일 열기 오류','오류/추적을 초기화하고 /APP002.FEZ만 open=FR_NO_FILE(4)로 만든 뒤 dump\\n을 입력한다.', "[ERROR] f_open 'APP002.FEZ' 포함 (0=없음,1=있음)",[1],'run_service')
    for c,length in enumerate([0,1,15,16,17,33],5):
        add(17,c,f'{length}바이트 파일과 16바이트 줄 경계',f'각 건 경계/오류를 초기화하고 4개 파일에 payload[i]=0x80+i (0≤i<{length})를 저장한다. 길이 0이면 데이터가 없다. dump\\r\\n을 입력하고 4개 파일 블록 전체를 독립 생성한 문자열과 비교한다.',
            '네 블록 전체 일치 (0=다름,1=같음);열린 파일 수 (개);모의 reset 횟수 (회)',[1,0,0],'run_service')
    for c,title,cmd,dumps,invalid in [(11,'빈 명령과 CR 무시',r'\r\n\n',0,0),(12,'백스페이스 편집',r'dumb\bp\r\n',1,0),(13,'명령 길이 초과 후 회복','x를 정확히 70개 보낸 뒤 \\ndump\\n',1,1),(14,'선두 백스페이스와 연속 dump',r'\b\bdump\ndump\n',2,0)]:
        add(17,c,title,f'경계/오류를 초기화하고 입력 `{cmd}`를 전달한다. 4개 파일은 C10의 33바이트 상태를 유지한다.',
            'FW DUMP BEGIN 출현 횟수 (회);invalid command. 출현 횟수 (회);열린 파일 수 (개)',[dumps,invalid,0],'run_service')
    add(17,15,'한 청크 뒤 읽기 실패와 다음 파일 계속','33바이트 파일 4개를 유지한다. 추적값을 초기화하고 두 번째 파일 읽기 호출만 FR_DISK_ERR(1)를 반환하게 한 뒤 dump\\n을 입력한다.',
        'MANIFEST의 앞 16바이트와 마커 일치 (0=다름,1=같음);MANIFEST 오류 문구와 APP002 33바이트 블록 모두 존재 (0=아님,1=맞음);열린 파일 수 (개)',[1,1,0],'run_service')
    add(17,16,'읽기 오류 해제 후 정상 재출력','모든 오류와 추적값을 초기화하고 같은 33바이트 파일들에 dump\\n을 다시 입력한다.',
        'MANIFEST 33바이트 블록 일치 (0=다름,1=같음);[ERROR] 문구 없음 (0=있음,1=없음);열린 파일 수 (개)',[1,1,0],'run_service')
    add(18,1,'서비스 표시와 재기동 요청','표시 반복값/색을 보관하고 공개 NVM 및 12 34의 4개 파일을 준비한다. 최소 서비스 반복=380, RTT 입력=reboot\\n으로 service_main을 호출한다.',
        '모의 reset 호출 횟수 (회);호출 전보다 GPIO 쓰기 증가 (0=아님,1=증가);모의 코어 기동 횟수 (회)',[1,1,0],'run_service',
        behavior='500회 1 ms 지연 요청과 LED tick 뒤 reset 경계를 1회 호출한다. PC에서는 실제 reset과 실제 500 ms 경과를 수행하지 않는다.')
    add(19,1,'PC 상태 파일 없음','새 임시 루트에서 /BOOT_STATUS.TXT가 없도록 시작하고 snd_boot_handle_file 호출 뒤 저장 파일을 다시 읽는다.',
        '선택 슬롯;저장 state;저장 boot_slot_num',[255,1,255],'run_slot')
    add(19,2,'PC 3바이트 불완전 상태','C1의 상태 구조체 첫 3바이트만 파일에 저장하고 처리 후 다시 읽는다.', 'boot_slot_num',[255],'run_slot')
    add(19,3,'PC 64바이트 정상 읽기','C2 기본 상태의 state=1, boot_slot_num=1, crc32=0xDEADBEEF로 바꾸고 64바이트를 저장한 뒤 처리한다.', '선택 슬롯',[1],'run_slot')
    add(19,4,'PC CRC 필드 보존 관찰','C3 처리 후 파일을 다시 읽고 crc32를 확인한다.', 'crc32 (uint32 bit pattern)',[0xdeadbeef],'run_slot',characterization=True,
        behavior='비교 통과는 필드 보존의 특성화다. CRC 무결성 검사가 통과했다는 뜻이 아니다.')
    for c,title,proc,expected in [(101,'RAM 파일 없음','exists=0, size=0, bytes[0..63]=0',[255,1,255,1,4096,1]),
        (102,'RAM 3바이트 읽기','exists=1, size=3, bytes[0..63]=0',[255,1,255,1,4096,1]),
        (103,'RAM 읽기 오류','exists=1, size=64, state=1/boot_slot_num=1/crc32=0xDEADBEEF 이외 0인 구조체, read_fault_once=1',[255,1,255,1,4096,1]),
        (104,'RAM 유효 상태 유지','C103과 같은 64바이트 입력, read_fault_once=0',[1,1,1,0,64,1])]:
        add(19,c,title,'매 건 RAM 파일 상태를 새로 0 초기화한 뒤 '+proc+'으로 설정하고 snd_boot_handle_file을 호출한다.',
            '선택 슬롯;저장 state;저장 boot_slot_num;RAM write 횟수 (회);논리 파일 크기 (byte);handle 닫힘 (0=열림,1=닫힘)',expected,'run_ram_slot',ram=True)
    add(19,105,'RAM CRC 필드 특성','C104 직후 result.crc32를 읽는다.', 'crc32 (uint32 bit pattern)',[0xdeadbeef],'run_ram_slot',ram=True,characterization=True,
        behavior='CRC 계산이나 유효 판정이 없다. 저장 필드 보존만 확인한다.')
    for c,length in enumerate([0,1,63,64,4096],106):
        add(19,c,f'RAM 파일 크기 {length}바이트와 반복',f'RAM 파일 구조를 0으로 초기화한다. 입력 상태 64바이트는 state=1, boot_slot_num=4만 설정하고 나머지는 0이다. exists=1, 논리 size={length}로 처리한다. 결과 전체 64바이트를 독립 기본값/입력값과 비교한다. 이어 write 누계를 저장하고 같은 handler를 재호출한다.',
            '최종 선택 슬롯;첫/둘째 처리의 전체 64바이트가 기대와 동일 (0=다름,1=같음);둘째 호출 추가 write 없음 (0=추가,1=없음);handle 닫힘 (0=열림,1=닫힘)',
            [255 if length<64 else 4,1,1,1],'run_ram_slot',ram=True,
            behavior='길이가 64 미만이면 문서의 기본 상태 64바이트, 64 이상이면 입력 상태 64바이트를 유지해야 한다. 4096은 논리 길이이며 RAM 자료 배열은 64바이트다.')
    states=[(1,1,0,3,[1,0,1,1,3]),(2,2,2,3,[2,3,2,2,3]),(2,2,3,3,[1,4,3,1,3]),
            (2,3,0,2,[2,0,3,2,2]),(2,3,0,3,[1,0,4,2,3]),(2,99,0,3,[1,0,4,2,3]),(99,1,0,3,[1,0,4,99,3])]
    for ram in [False,True]:
        for i,(state,sub,tries,result,expected) in enumerate(states if ram else states[:6],1):
            title=['기본 슬롯','대체 세 번째 시도','대체 시도 상한 초과','대체 성공 유지','대체 실패 후 복귀','미지원 하위 상태','미지원 전체 상태'][i-1]
            fields='선택 슬롯;저장 alt_boot_try_count (회);저장 sub_state;저장 state;저장 alt_boot_result'
            if ram: fields+=';handle 닫힘 (0=열림,1=닫힘)'
            add(20,i+100 if ram else i,title,
                f'각 건 상태 64바이트를 0으로 초기화하고 boot_slot_num=1, alt_boot_slot_num=2, state={state}, sub_state={sub}, alt_boot_try_count={tries}, alt_boot_result={result}로 저장한다. '+
                ('RAM 파일 exists=1, size=64로 초기화하고 handler를 실행한 뒤 RAM 자료를 읽는다.' if ram else '새 값으로 파일을 덮은 뒤 handler 실행과 파일 다시 읽기를 수행한다.'),
                fields,expected+([1] if ram else []),'run_ram_slot' if ram else 'run_slot',ram=ram)


KEY = 'key[0..3]={0x2B7E1516, 0x28AED2A6, 0xABF71588, 0x09CF4F3C}'
CRC_RULE = '''CRC는 독립 CCITT 식을 사용한다. 초기값 0xFFFF, 다항식 0x1021, 비트 반사 없음, 최종 XOR=0이다. 바이트마다 CRC 상위 8비트에 입력을 XOR하고 8회 왼쪽 이동하며 최상위 비트가 1이면 0x1021을 XOR한다. 매번 하위 16비트만 유지한다. 파일 헤더 끝의 CRC는 상위 바이트부터 저장한다. 데이터 CRC 인자는 이 16비트 결과의 바이트 순서를 바꾼 값이다. boot_info와 제조 표의 CRC 필드는 little-endian 정수로 저장하므로 파일 헤더와 구분한다.'''
FLAG_RULE = '''구획 flags는 비트 조합이다. bit7=0이면 평문, 1이면 AES 암호화(0x80), bit6=0이면 CRC 검사, 1이면 VOLATILE로 일반 데이터 전송의 CRC 오류를 무시(0x40), bit5=0이면 추가 잠금 없음, 1이면 PRAM 잠금 요청(0x20), bit4=0이면 파일 데이터, 1이면 데이터 없는 0 채움(0x10)이다. 본문에서 별도 지정하지 않은 flags/offset/aes_pad_count/예약값은 모두 0이다. size와 offset은 32비트 워드 단위다. 일반 데이터 구획의 자료 길이는 size×4바이트이며 EMPTY는 파일을 읽지 않는다. 기동 구획은 예외로 load_boot_section이 size와 무관하게 평문 8바이트 또는 암호문 16바이트를 읽고 CRC 확인 후 첫 8바이트를 {stack_pointer, init_pointer}로 출력한다. 암호화 기동 구획 BOOT-011 C112는 size=2여도 16바이트를 읽는다. 기동 구획은 VOLATILE 비트에 따른 CRC 생략 분기도 없다. stack_pointer/init_pointer는 출력 주소이며 메모리 전송을 뜻하지 않는다.'''
SLOT_RULE = '''상태 파일은 /BOOT_STATUS.TXT이며 little-endian packed 64바이트다. state=1은 기본 기동, 2는 대체 기동이다. sub_state=1은 IDLE(대기), 2는 BOOT_TRY(시도 중), 3은 BOOT_TRY_DONE(시도 종료), 4는 UNKNOWN(알 수 없음)이다. alt_boot_result=1은 결과 없음, 2는 성공, 3은 실패다. slot_n_state=1은 사용 정보 없음, 2는 사용 가능이다. 이 열거값은 불리언 0/1 플래그가 아니다. 99는 미지원 상태 주입값이다. 시도 상한은 3회이며 증가 후 3을 넘으면 기본 슬롯으로 복귀한다. 슬롯 0/255는 파일 경로 선택에서 루트를 사용하며 1..4는 해당 하위 폴더다. 슬롯 자체의 범위 검증을 증명하는 벡터는 없다.

| 바이트 인덱스 | 필드 | 기본 상태 값 |
|---|---|---|
| 0 | major_ver | 0 |
| 1 | minor_ver | 1 |
| 2 | state | 1 |
| 3 | sub_state | 1 |
| 4 | boot_slot_num | 255 |
| 5 | last_boot_slot_num | 255 |
| 6 | alt_boot_slot_num | 0 |
| 7 | alt_boot_try_count | 0 |
| 8 | alt_boot_result | 1 |
| 9, 10, 11, 12 | slot_1_state, slot_2_state, slot_3_state, slot_4_state | 각각 1 |
| 13..59 (47바이트 전부) | reserved[0..46] | 각각 0 |
| 60..63 (4바이트 전부) | crc32 | 각각 0 |

각 시나리오의 ‘0 초기화 상태’는 위 기본 상태와 다르다. 전 64바이트를 0으로 한 뒤 명시한 필드만 설정한다. 0xDEADBEEF인 crc32의 byte[60..63]은 EF BE AD DE다. 파일 예약 길이 4096바이트와 유효 상태 구조체 64바이트를 구분한다. RAM 모형은 64바이트 배열과 논리 길이만 가지며 byte[64..4095]의 물리 저장을 만들거나 비교하지 않는다.'''


def hexbytes(data: bytes) -> str:
    return ' '.join(f'{x:02X}' for x in data)


def data_table(data: bytes, label: str) -> str:
    rows=[f'| {label} 바이트 인덱스 | 값 (16진수, 왼쪽부터 연속 인덱스) |','|---|---|']
    for start in range(0,len(data),16):
        rows.append(f'| {start}..{min(start+15,len(data)-1)} | {hexbytes(data[start:start+16])} |')
    return '\n'.join(rows)


def seal(data: bytes) -> bytes:
    return data[:-2] + crc16(data[:-2]).to_bytes(2,'big')


def app_header(entries: int = 0, ident: int = 0x657a6169) -> bytes:
    return seal(struct.pack('<IH',ident,entries) + bytes(2*entries + 2*(entries % 2) + 2))


def section_header(kind: int, size: int, flags: int = 0) -> bytes:
    return seal(struct.pack('<IIHHBBH',0,size,kind,flags,0,0,0))


def encrypted_words(data: bytes) -> bytes:
    """공개 fixture를 별도 AES 구현으로 계산한다. 제품 출력은 읽지 않는다."""
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    key=bytes.fromhex('2b7e151628aed2a6abf7158809cf4f3c')
    swapped=b''.join(data[i:i+4][::-1] for i in range(0,len(data),4))
    enc=Cipher(algorithms.AES(key),modes.ECB()).encryptor()
    output=enc.update(swapped)+enc.finalize()
    return b''.join(output[i:i+4][::-1] for i in range(0,len(output),4))


def boot_fixture() -> str:
    info=bytearray(32)
    info[2:4]=b'A\0'; info[12:16]=b'FEZ\0'; info[17]=1
    struct.pack_into('<IHH',info,20,0x12345678,1024,64)
    struct.pack_into('<H',info,30,crc16(info[:30]))
    manu=bytearray(256); struct.pack_into('<I',manu,184,0x11223344)
    struct.pack_into('<H',manu,254,crc16(manu[:254]))
    return f'''기동정보는 아래 32바이트다. 필드는 boot_fs_ver=0, boot_app_load="A", boot_app_ext="FEZ", boot_media_id=0, boot_speed=1(SSPI), boot_sec=0, boot_addr=0x12345678, boot_manu_off=1024 byte, boot_manu_len=64 word, reserved[0..1]=0이다. speed=0은 AUTO, 2는 DSPI, 3은 QSPI, 4..255는 예약값이다. 이름 배열은 10바이트 중 [0]='A', [1..9]=0이고 확장자 배열은 [0..3]={{'F','E','Z',0}}다.

{data_table(bytes(info),'기동정보')}

PC NVM[0..2047]는 모두 0으로 시작하고 [512..543]에 위 기동정보, [1024..1279]에 제조 표를 둔다. 다른 바이트는 모두 0이다. 제조 표 상대 byte[184..187]은 44 33 22 11(Reserved[0]), [254..255]는 {hexbytes(manu[254:256])}(앞 254바이트 CRC의 little-endian 값)이고 [0..183], [188..253]은 모두 0이다. 제조 정보는 총 64워드/256바이트로 생략된 입력이 없다. 실제 장치의 제조 키/보정값은 사용하지 않는다.

RAM boot-info 시험은 512바이트 배열의 [0..31]만 위 기동정보로 채우고 [32..511]은 모두 0이다. NVM offset=512 byte의 512바이트 읽기를 모사한다. out은 128개의 uint32 워드이며 초기값은 전부 0이다.

{CRC_RULE}'''


def define_text() -> None:
    INTRO.update({
        1:'응용 코어 실행 전 공통 GPIO/공유 영역 준비를 분석한다. 전체 main을 끝까지 실행하는 유한 PC 시험은 구현되지 않았다.',
        2:'지연 함수가 Sys_Delay에 전달하는 cycle 인자를 확인한다. PC 경계는 기다리지 않고 인자만 저장한다. 정수 나눗셈으로 먼저 절삭하므로 30.72 MHz의 1 us 요청은 30 cycle이며 30.72 cycle이 아니다. 이 값은 현재 계산의 특성이고 실제 시간 정확도 판정값이 아니다.',
        3:'CAL 핀의 안정된 입력에 따른 서비스/정상 기동 분기를 분석한다. CAL 활성 전기 레벨은 0, 비활성은 1이다. 논리적 서비스 조건 true/false와 핀의 숫자를 혼동하지 않는다.',
        4:'TDC_BOOT_LED_ENABLE=1로 빌드하고 타이머/GPIO를 PC 경계로 바꾼 시험이다. 하드웨어 인터럽트 대신 실제 ISR 함수를 호출한다. 40회/1 ms의 관계는 주입 조건이며 시간 실측이 아니다. phase=0 IDLE, 1 FADE_IN, 2 PEAK, 3 FADE_OUT, 4 OFF, 5 DONE이다. 시작 후 30 ms에 peak, 330 ms에 fade-out, 360 ms에 OFF, 다음 tick인 361 ms에 DONE으로 바뀐다. G/B duty의 단위는 100분율이며 최대 30/40이다. 부트 표시 GPIO는 1=HIGH/켜짐, 0=LOW/꺼짐이다.',
        7:'실제 tdc_boot_storage_init를 실행하되 NVM/trim/UART/파일시스템 경계를 PC에서 모사한다. 각 clear_trace는 오류/읽기/재설정/기동/전송/이벤트 카운터를 0으로 돌리고 cwd를 /로 한다. 열린 파일이 남아 있으면 초기화로 숨기지 않고 abort한다. fail_mask의 bit(k-1)=1은 k번째 NVM 읽기를 -9로 실패시키며 0은 해당 실패 미주입이다. FR_OK=0, FR_DISK_ERR=1이다. 양의 1도 오류일 수 있으므로 반환값의 부호만 보고 성공으로 해석하지 않는다.\n\n'+boot_fixture(),
        8:'기동정보 읽기/CRC는 RAM 콜백으로 수행할 수 있으나 제조 표 적재는 trim을 적용하므로 PC 보충 시험만 수행한다. 하드웨어의 전압/클럭 교정값 적합성은 시험하지 않는다.\n\n'+boot_fixture(),
        9:'로컬 기동정보와 20바이트 결과 배열만 사용한다. 배열 인덱스는 0부터이며 문자열 종료 NUL의 값은 0이다. 실패 후 버퍼 보존 여부는 C2~C5에서 비교하지 않는다.',
        10:'파일 전체 적재 C1~C9는 PC 임시 파일을 사용하며 RAM C101~C113은 헤더 파서만 호출한다. RAM read의 fail=0은 정상, 1은 항상 오류, fail_on_read=0은 회차 오류 없음, 양수 n은 n번째 read 오류다. 실패한 read는 read_count=0을 반환하며 파일 위치를 옮기지 않는다. 매 건 별도 지시가 없으면 유효한 FIL과 출력 배열 uint32[8] (32바이트)을 사용한다.\n\n'+CRC_RULE,
        11:'PC C1~C6은 전송 경계와 파일을 사용한다. RAM C101~C114는 기동정보 파싱만 하며 이미지 DMA나 실제 코어 기동은 수행하지 않는다. 아래 순서를 유지해 오류 후 상태를 확인한다.\n\n'+FLAG_RULE+'\n\n'+CRC_RULE,
        12:'목록은 ASCII이며 줄 끝 LF는 0x0A다. 기본 목록 byte[0..11]={41 2E 46 45 5A 0A 42 2E 46 45 5A 0A}, RAM 문자열 byte[12]=0(NUL)이다. CRC 대상은 NUL을 제외한 12바이트다. missing.FEZ\\n도 정확히 12바이트다. 목록 헤더 ident=0x657A616D, crc_table_size=1이다. 정상 대상 A.FEZ/B.FEZ는 ident=0x657A6169, 목록 0개의 8바이트 app 헤더이며 실제 코어를 시작하지 않는다. RAM read_error/missing_b/nested/chdir_error는 각각 0=미주입, 1=이름에 해당하는 실패/중첩 조건 주입이다.\n\n'+CRC_RULE,
        13:'실제 CRC의 워드/나머지 처리와 헤더 복호화 분기를 실행한다. PC에서는 CRC 주변장치 경계만 CCITT 모형으로 바뀐다. 타깃에서 이 경로를 실행하면 실제 CRC 엔진을 사용하므로 RAM_ONLY PC 성공을 실기 CRC 통과로 확대하지 않는다.\n\n'+FLAG_RULE+'\n\n'+CRC_RULE,
        14:'AES 시험은 NIST SP 800-38A F.1에 해당하는 공개 한 블록 벡터이며 시험 전용 키다. 제품 키를 읽거나 로그에 넣지 않는다. 방식은 AES-128 ECB, IV 없음이다. '+KEY+'. ciphertext[0..3]={0x3AD77BB4,0x0D7A3660,0xA89ECAF3,0x2466EF97}, 사전 plaintext[0..3]={0x6BC1BEE2,0x2E409F96,0xE93D7E11,0x7393172A}다. 인덱스 순서는 왼쪽부터 0..3이고 각 값은 uint32 워드다. API는 워드 바이트 순서를 변환해 AES에 전달한 뒤 다시 복원한다. 0xE93D7E11은 signed int 로그에서 -381845999로 보이지만 같은 비트값이다.',
        15:'입력 data word[0..3]={0x00000011,0x00000022,0x00000033,0x00000044}이며 파일 byte[0..15]={11 00 00 00 22 00 00 00 33 00 00 00 44 00 00 00}다. '+FLAG_RULE+' DMA mode=0은 32→32비트, 0x58000(360448)은 8→24비트다. X/Y의 count=16은 바이트 전송 수이고 P/CM3/IOMEM의 count=4는 32비트 워드 수다. PC 모형은 입력 원시 바이트를 보관할 뿐 24비트 목적 메모리의 실제 패킹을 수행하지 않는다.',
        16:'lock.bin의 uint32 word[0..3]={0x11,0x22,0x33,0x44}이며 byte[0..15]={11 00 00 00 22 00 00 00 33 00 00 00 44 00 00 00}다. '+FLAG_RULE+' 실제 함수가 요청한 잠금 설정 순서를 PC 추적으로 비교한다. 레지스터 접근 제한이 실제 적용되었는지는 확인하지 않는다.',
        17:'RTT 입력을 PC 문자열 큐로 넣고 실제 서비스 파서와 출력 포맷을 실행한다. 파일은 PC 임시 루트에만 있다. 대상 순서는 /MANIFEST.TXT, /APP000.FEZ, /APP001.FEZ, /APP002.FEZ이다. 서비스 루프는 PC에서만 입력 큐와 추가 tick 소진 후 반환한다. 장치의 무한 서비스 루프나 RTT 물리 전송은 실행하지 않는다. \\r=CR(13), \\n=LF(10), \\b=백스페이스(8)다. 일반 명령 버퍼는 64바이트, 수신 문자는 최대 62개까지 저장한다. 오류 주입은 별도 지시가 없으면 모두 0(꺼짐)이다. 파일 없음=FR_NO_FILE(4), 읽기 실패=FR_DISK_ERR(1)이다.\n\n'+boot_fixture(),
        18:'service_main을 PC에서 유한 실행한다. 서비스 LED는 380번 tick마다 색 번호를 (color+1)&7로 바꾼다. 색 bit0/1/2가 각각 R/G/B이며 이 서비스 출력의 GPIO는 비트가 1이면 0(LOW), 비트가 0이면 1(HIGH)을 요청한다. BOOT-004의 부트 PWM과 극성을 같은 것으로 취급하지 않는다. C1의 payload는 GPIO 쓰기 증가만 비교하며 색 순서 전체나 물리 밝기는 판정하지 않는다.\n\n'+boot_fixture(),
        19:'상태 파일 존재/읽기 길이에 따른 초기화를 확인한다. 쓰기 실패와 전원 중단은 자동 주입하지 않는다. 아래 64바이트 표로 입력과 기대 레코드를 완전히 복원할 수 있다.\n\n'+SLOT_RULE,
        20:'PC 임시 파일과 RAM 파일 두 환경에서 같은 상태 규칙을 비교한다. 각 건 입력을 새로 만들므로 이전 건의 시도 횟수를 가져오지 않는다.\n\n'+SLOT_RULE,
    })
    INTRO[5]='CM3의 SP 설정/시작 주소 분기와 CFX mailbox 기동을 정적 분석한다. PC의 bootloader_boot_cfx/cm3는 횟수만 세는 대체 함수이므로 실제 응용 코어 실행의 PASS 근거가 아니다.'
    INTRO[6]='저장장치 준비, 이름/슬롯 선택, 헤더/구획 검사, 잠금과 응용 기동의 순서를 분석한다. 전체 bootloader_boot 정상/실패 경로를 유한 실행하는 시험은 없다.'
    for n in range(1,21):
        SOURCES[n]=[PRODUCT]
    for n in [1,3]: SOURCES[n]=['src/0__bootloader/main.c','src/0__bootloader/include/main.h']
    SOURCES[2]=['src/0__bootloader/source/tdc_util.c']
    SOURCES[4]=['src/0__bootloader/source/led/tdc_boot_led.c']
    SOURCES[5]=['src/0__bootloader/source/bootloader_boot_cores.c',PRODUCT]
    for n in [17,18]: SOURCES[n]=['src/0__bootloader/source/service/service.c',PRODUCT]
    for n in [19,20]: SOURCES[n]=['src/0__bootloader/source/bootmgr/ci_boot.c','src/0__bootloader/include/sdk_ci_boot.h']
    SOURCES[13].append('src/0__bootloader/source/bootloader_crc.c')
    SOURCES[14].append('src/0__bootloader/tiny-AES-c/aes.c')
    LIMITS.update({
        1:['main 전체 유한 실행/응용 기동 전 공유 메모리 실측은 미구현/미실행이다. 초기 GPIO 일부는 인터럽트 마스크 전 실행되므로 전 초기화를 인터럽트 차단 상태라고 단정하지 않는다.'],
        2:['실제 Sys_Delay 경과 시간, 함수 오버헤드, 최대 uint32 곱셈 오버플로는 미검증이다. 실측 허용오차가 정의되지 않아 시간 정확도 판정은 보류한다. 타깃 자동 벡터는 없다.'],
        3:['첫 CAL 읽기 0(활성), 두 번째 1(비활성)이면 서비스/정상 어느 쪽도 호출하지 않고 main 끝으로 간다. 이 경계의 올바른 복구 정책은 미정이며 통과로 간주하지 않는다.'],
        4:['실제 Timer2 주기/PWM 파형/GPIO 전압/응용 LED와의 충돌은 미측정이다. stop 이후 ISR이 강제로 호출되는 비정상 조건은 시험하지 않는다. 타깃 자동 실행은 미지원, observer만 사용 가능하다.'],
        5:['실제 SP 변경/BX, mailbox 응답/CFX_CMD 실행은 미실행이다. CFX init_pointer 경계는 코드 분석이며 독립 실행 벡터가 아니다.'],
        6:['저장 초기화의 양의 FatFs 오류를 bootloader_boot의 ret<0 조건이 차단하지 못할 수 있다. 제조 표 오류 무시는 BOOT-007의 두 실패로 재현된다. f_unmount 오류는 로그 후 기동을 계속한다. 전체 오류 전파는 충족이 확인되지 않았다.'],
        7:['C6 기대 -1/관찰 0: 제조 CRC 오류가 무시된다. C17 기대 [-9,1,0]/관찰 [0,0,0]: 제조 읽기 오류를 무시하고 출력까지 게시한다. 제품 수정은 수행하지 않았다.',
           '타깃에서 NVM 설정, 전압/클럭 trim, UART, mount/sync를 격리할 경계가 부족하여 PC 전용이다. 실제 NVM/보정/전원 동작은 미실행이다. AUTO의 정상 첫 QSPI 성공 등 모든 속도 조합을 포괄한 것은 아니다.'],
        8:['RAM 시험은 boot-info 읽기/CRC만 확인한다. 제조 trim의 전체 필드 매핑, 제조 표 all-zero/all-one 거부 실행 및 실제 교정값은 이 입력에 포함되지 않는다.'],
        9:['최대 이름 길이, NUL 없는 배열, 출력 용량 부족은 함수 인터페이스의 한계와 함께 미시험으로 남긴다. 작은 버퍼를 실제 주입해 메모리 손상을 유발하지 않는다.'],
        10:['처음 8바이트보다 작은 출력 용량 및 암호화 첫 읽기의 20바이트보다 작은 출력 용량은 안전한 제품 검사가 확립되지 않아 실행하지 않았다. NULL 출력, 큰 암호화 CRC 목록, 실파일 I/O는 RAM 범위 밖이다.'],
        11:['C6/C108은 선언 섹션 누락, C109는 첫 헤더 read 오류를 무시해 0을 반환한다. C110은 CFX 파싱 후 다음 헤더 read 오류를 무시해 부분 플래그 1을 반환한다. 모두 기대 -1을 유지한다. C110의 부분 CFX 출력 처리/폐기 정책은 판정 보류다.',
            '0보다 크고 16보다 작은 구획 헤더는 미초기화 바이트 참조 가능성 때문에 미주입이다. 물리 DMA, 메모리 패킹, 코어 시작, VOLATILE/EMPTY/LOCKED 모든 조합과 다중 대형 전송은 미검증이다.'],
        12:['닫힌 목록의 마지막 재읽기 오류가 무시된다. C106의 closed_reads=1은 알려진 특성이다. 512바이트 이상/다중 블록/암호화 목록/긴 파일명/구분자 없는 최대 길이 입력은 검증되지 않았다.'],
        13:['CRC 일치는 전송 무결성 확인이며 송신자 인증/암호 안전성의 증명이 아니다. 실기 CRC 주변장치/경합/정렬 위반/NULL 데이터의 실제 실행은 미검증이다.'],
        14:['공개 한 블록과 잘못된 len=3만 확인한다. 실제 키 선택/보관, 다중 블록, len=0, NULL 데이터/복호화 문맥을 모든 조건에서 보장하지 않는다.'],
        15:['목적 주소는 PC에서 역참조하지 않는다. 물리 DMA/24비트 패킹/대형 분할/패딩/시간 초과는 미검증이다. 타깃 자동 벡터가 아닌 observer 경계이며 이 문서는 장치 전송을 지시하지 않는다.'],
        16:['한 CFX PRAM0 입력의 잠금 순서만 비교한다. PRAM 경계 통과/CM3 잠금/실제 접근 차단은 미검증이다. 타깃에서는 관찰만 가능하다.'],
        17:['실제 RTT drain/대역폭/블로킹, 물리 NVM, 서비스 저장 준비 실패의 무한 대기는 자동 실행하지 않는다. dump/reboot는 접두어 비교이므로 뒤에 추가 문자가 있는 명령의 거부 정책은 현재 벡터로 증명되지 않는다.'],
        18:['실제 LED 전압/색/주기와 리셋 후 main 재진입은 미실행이다. 500 ms는 요청 반복으로만 분석하며 실측 허용오차와 RTT 도달 보장은 없다. 타깃 runner는 reboot를 자동 요청하지 않는다.'],
        19:['crc32를 검증하지 않는다. CRC 필드 보존은 무결성 PASS가 아니다. write/seek/sync/open 재확인 오류는 정지/무한 루프 위험과 기존 오류 무시 때문에 주입하지 않았다. 물리 전원 중단/원자성/내구성은 미검증이다.'],
        20:['슬롯 1..4 유효성 검사, 재시도 255 overflow, CRC 무결성, 실제 리셋 후 파일 지속성/전원 중단은 검증되지 않았다. 미지원 전체 state=99는 그대로 남고 sub_state만 UNKNOWN이 되는 현재 특성을 기록한다.'],
    })


ANALYSIS = {
    1: [('정상 기동의 초기화 순서',
         'CAL=1(비활성), 유효 저장 이미지가 있다는 분석 조건에서 main의 실행 순서를 확인한다. 실장치 입력은 인가하지 않는다.',
         '초기 DIO/디버그 설정 후 PRIMASK로 인터럽트를 차단하고 NVIC disable/clear 후 다시 허용한다. QCC_CTRL과 QCC_ISD_CHECK는 LOW(0)를 설정한다. 일반 분기에서 공유 PRAM5의 int[0]을 0으로 만든 뒤 bootloader_main을 호출해야 한다. 이 0은 CFX EEPROM 자료가 아직 적재되지 않았음을 나타낸다.',
         '순서는 코드에서 확인된다. 실제 공유 메모리 준비/전기 상태와 응용 코어보다 앞선 실행의 실기 관찰은 없다.')],
    3: [('안정된 서비스 입력', 'CAL 핀을 연속 두 번 0(활성)으로 읽는 조건을 분석한다. 첫 읽기 뒤 tdc_delay_us(10)을 호출한다.',
         'CAL을 no-pull 입력으로 바꾸고 service_main만 선택한다. 정상 분기의 공유 int[0] 초기화와 bootloader_main은 호출하지 않는다.', '정적 분기 확인만 수행했다.'),
        ('안정된 정상 입력', '첫 CAL 읽기가 1(비활성)인 조건을 분석한다.',
         'CAL을 no-pull로 바꾸고 공유 int[0]=0 후 bootloader_main을 선택한다. 서비스 함수는 호출하지 않는다.', '정적 분기 확인만 수행했다.'),
        ('두 번의 CAL 읽기 불일치', '첫 CAL=0, 두 번째 CAL=1인 조건을 분석한다.',
         '현재 코드 관찰: 두 부트 함수 모두 호출되지 않고 main에서 0을 반환한다. 요구되는 복구/재시도 동작의 기대값은 정의되지 않았다.', '관찰 및 판정 보류. 정상 기동 성공으로 기록하지 않는다.')],
    5: [('CM3 시작 정보 전달', '적재가 완료된 CM3 이미지의 stack_pointer/init_pointer를 분석 입력으로 사용한다. 주소값은 이미지가 소유하며 이번에 임의의 실행 주소를 주입하지 않는다.',
         'bootloader_boot_cm3가 SP에 stack_pointer를 넣은 뒤 BX init_pointer로 분기한다. CM3 flag=0xA5(SHOULD_BOOT)일 때만 호출되어야 한다.', '실제 함수의 명령과 호출 순서 분석, PC 대체 기동 횟수는 실기 증거가 아니다.'),
        ('CFX 사전 조건과 주소 경계', 'mailbox 두 번째 워드가 PCREQ인지와 init_pointer 범위를 확인한다. 하한 0x8000, 상한 0x18000이며 0x7FFF/0x18001 또는 PCREQ 불일치를 분석한다.',
         '사전 조건 실패이면 -1, 유효 범위이면 해당 메모리를 접근 가능하게 하고 시작 주소/PCKEY를 게시한 뒤 CFX_CMD_0을 요청하고 0을 반환한다. PCREQ/PCKEY는 SDK mailbox 상수이며 이번 시험 입력의 숫자로 사용하지 않는다.', '코드 분석만 있다. CFX가 실제로 응답해 실행했다는 관찰은 없다.')],
    6: [('정상 준비와 적재 후 기동', '저장 준비 0, 이름 생성 양수, 슬롯 결정, 정상 헤더/구획, NVM 동기화 성공이라는 분석 조건이다.',
         '슬롯 선택 후 app 파일을 열고 헤더/구획을 처리한다. 필요한 잠금은 구획 전송 뒤 적용하고, 적재한 CFX 기동 자료가 있으면 CFX를 먼저 호출한 다음 CM3를 호출한다. cfx_boot_flag/cm3_boot_flag는 0이면 기동 요청 없음, 0xA5(SHOULD_BOOT)이면 기동 요청 있음이다. 구획 적재 반환 비트 1(CFX)/2(CM3)와 다른 값이다.', '전체 실행 벡터는 없다. 순서 분석만 수행했다.'),
        ('실패 후 기동 차단', '헤더/구획 반환 -1, 저장 준비의 양의 FR_DISK_ERR=1, 제조 정보 실패, unmount 실패의 네 분기를 따로 분석한다.',
         '요구되는 성공 조건은 유효 적재 이후에만 기동하는 것이다. 헤더/구획 음수 실패는 조기 반환하지만 저장 준비의 양수 오류, 제조 오류 무시, unmount 오류 후 진행은 이 조건의 충족을 입증하지 못한다.',
         'BOOT-007/011의 알려진 실패와 구분해 남긴 정적 한계다. 기대값을 현재 오류 동작으로 바꾸지 않는다.')]
}


def binary_details(unit: int) -> str:
    text=[]
    if unit in (10,12):
        text += ['### 파일 헤더의 전체 바이트',
                 '평문 무섹션 입력과 목록 한 항목 헤더를 아래처럼 만든다. 인덱스는 각각 0부터다. 명시한 손상/길이 제한을 적용하기 전의 정상 자료다.',
                 data_table(app_header(),'평문 0개'), data_table(app_header(1),'평문 1개')]
        if unit==12:
            for label,data in [('기본 목록',b'A.FEZ\nB.FEZ\n'),('실패 목록',b'missing.FEZ\n')]:
                crc=crc16(data); text.append(f'{label} data_crc 인자=0x{int.from_bytes(crc.to_bytes(2,"big"),"little"):04X}.')
        else:
            plain=app_header(0,0x455a4149)+bytes(12)
            encrypted=plain[:4]+encrypted_words(plain[4:])
            text += ['암호화 헤더는 ident만 평문으로 두고 나머지 16바이트를 AES-128 ECB로 암호화한다. '+KEY+'. 원래 20바이트의 [8..19]는 모두 0이며 AES 전후 각 uint32 워드에 byte-swap을 적용한다.',
                     data_table(plain,'암호화 전'),data_table(encrypted,'암호화 입력 전체')]
    if unit==11:
        cfx=struct.pack('<II',0,0x23456); cm3=struct.pack('<II',0x20007ff0,0x12345)
        full=section_header(8,2)+cfx+section_header(9,2)+cm3
        text += ['### 기동 구획과 암호문의 전체 입력',
                 '48바이트 자료에서 [0..15]는 CFX 헤더, [16..23]는 CFX 기동 자료, [24..39]는 CM3 헤더, [40..47]는 CM3 기동 자료다. 구획 헤더 필드는 offset[0..3], size[4..7], type[8..9], flags[10..11], aes_pad_count[12], reserved[13], header_crc[14..15] 순서다.',data_table(full,'두 구획')]
        plain=struct.pack('<IIII',0x20007ff0,0x12345,0x11223344,0x55667788)
        text += [KEY+'. 암호화는 각 워드의 바이트 순서를 바꿔 AES-128 ECB에 넣고 암호문 워드도 다시 바이트 순서를 복원한다. 복호화 기대 기동 자료는 첫 두 워드다.',data_table(plain,'암호화 전'),data_table(encrypted_words(plain),'암호화 입력 전체')]
        for label,data in [('일반 전송',struct.pack('<IIII',0x11,0x22,0x33,0x44)),('CFX 기동',cfx),('CM3 기동',cm3),('암호화 기동 평문',plain)]:
            crc=crc16(data); text.append(f'{label}의 data_crc 인자=0x{int.from_bytes(crc.to_bytes(2,"big"),"little"):04X}.')
    if unit==13:
        header=section_header(3,4)
        encrypted=section_header(3,4,0x80)
        damaged=bytearray(encrypted); damaged[0]^=1
        text += ['### 헤더 입력 전체',KEY+'. 헤더는 uint32[4]에 해당하는 16바이트다. 일반 필드는 little-endian, 끝 CRC 2바이트는 big-endian이다. 암호화는 워드별 byte-swap → AES-128 ECB → 워드별 byte-swap 순서다.',
            data_table(header,'평문 정상'),data_table(encrypted,'암호화 전 flags=0x80'),
            data_table(encrypted_words(encrypted),'암호화 flags=0x80 입력'),
            data_table(encrypted_words(header),'암호화 flags=0 입력'),
            data_table(encrypted_words(bytes(damaged)),'암호화 CRC 손상 입력')]
    if unit in (15,16):
        crc=crc16(struct.pack('<IIII',0x11,0x22,0x33,0x44))
        text.append(f'표준 16바이트 자료의 독립 CRC=0x{crc:04X}, data_crc 인자=0x{int.from_bytes(crc.to_bytes(2,"big"),"little"):04X}다. '+CRC_RULE)
    if unit==17:
        text += ['### 구현된 서비스 로그 형식과 전체 payload',
            'payload[0..32]는 아래와 같다. 길이 L 시험에서는 [0..L-1]만 파일에 저장한다. 0바이트는 데이터 행 없이 BEGIN 다음 END가 온다.',data_table(bytes(range(0x80,0xa1)),'payload'),
            '파일 블록의 시작은 `:::FILE:<이름>:BEGIN:::`+LF, 데이터는 바이트당 대문자 2자리 16진수와 공백 1개, 16바이트마다 LF, 마지막 짧은 행도 LF, 끝은 `:::FILE:<이름>:END:::`+LF다. 네 파일 모두 같은 규칙을 적용한다. 전체 블록 앞/뒤는 `========== FW DUMP BEGIN ==========`+LF와 `========== FW DUMP END ==========`+LF다. 콘솔 안내와 `[SERVICE] dump done.`은 CRLF다. 다음은 실제 구현 형식의 MANIFEST 33바이트 예상 블록이다.',
            '```text\n:::FILE:MANIFEST.TXT:BEGIN:::\n80 81 82 83 84 85 86 87 88 89 8A 8B 8C 8D 8E 8F \n90 91 92 93 94 95 96 97 98 99 9A 9B 9C 9D 9E 9F \nA0 \n:::FILE:MANIFEST.TXT:END:::\n```',
            'C15의 첫 파일은 첫 데이터 행 뒤 즉시 END와 `[ERROR] f_read \'MANIFEST.TXT\' failed (1)`+LF가 온다. 다음 파일들은 정상으로 계속 읽는다. C4는 `[ERROR] f_open \'APP002.FEZ\' failed (4)`+LF만 출력하고 그 파일의 BEGIN/END는 생략한다. 이 문자열은 PC에서 캡처한 구현 출력의 비교 규칙이며 장치 RTT에서 관찰했다는 뜻이 아니다.']
    if unit==18:
        text += ['### 구현된 재기동 안내',
                 '`[SERVICE] reboot requested. Resetting in 500ms...` 뒤 CRLF가 출력된다. 자동 payload는 이 문구 자체나 500 ms 실시간을 비교하지 않고 reset 호출 수/GPIO 변화/기동 없음만 비교한다. 이후 실제 보드 리셋 성공 로그는 구현된 것으로 만들지 않는다.']
    return '\n\n'.join(text)


def boot_meta(unit: int, has_ram: bool) -> str:
    if unit in ANALYSIS:
        base='이 ID는 HOST 및 RAM_ONLY 두 모드와 O0/O2 모두 반환 -2(UNSUPPORTED), count=0, done=0, overflow=0이어야 한다. 이 미지원 결과의 확인은 제품 유닛 PASS가 아니다. 시험 실행기가 실제로 반복 주입한 ID 범위는 1..20이며 ID 0/21을 별도 실행했다는 근거는 없다.'
    else:
        base='완료된 결과를 release하지 않고 같은 ID를 다시 호출하면 -3(BUSY)를 반환해야 한다. count와 actual/expected 각각의 전체 128개 int가 모두 직전과 같아야 한다. ret=0은 실행 완료, done=1은 결과 기록 완료이며 제품 PASS를 뜻하지 않는다. overflow는 0이어야 한다. O0와 O2에서 모든 record의 메타데이터/actual/expected/판정이 동일해야 한다.'
        if not has_ram:
            base+=' RAM_ONLY에서는 이 ID가 -2, count=0, done=0, overflow=0인 미지원 상태여야 한다.'
        else:
            base+=' RAM_ONLY에서는 release 후 같은 ID를 재실행하여 count와 유효 구간 actual/expected가 첫 실행과 완전히 같아야 한다. 반복은 같은 벡터이므로 본문에 두 번 복제하지 않는다.'
    base+='\n\nHOST에서 ret=0인 실행의 종료 시 read/context, NVM read/context, 파일 open/close/stat/write/seek/sync/chdir/context, observer와 public_key override가 모두 NULL로 복원되어야 한다. `clear_trace`는 열린 파일 수가 0일 때만 가능하다. BOOT-002는 주입 클럭을, BOOT-017/018은 서비스 iter/color를 호출 전 값으로 복원한다. BOOT-019/020은 private 상태/선택 슬롯/FIL 포인터를 복원한다.'
    base+='''

RAM_ONLY의 복원 시험은 기존 상태를 0이 아닌 다음 값으로 준비한다. 파일/NVM/observer의 이전 콜백은 호출되면 abort하는 sentinel이며 context는 같은 marker 주소다. 공개 시험 키 이전 포인터는 words[0..3]={1,2,3,4} 배열, 이전 슬롯=3, 상태 64바이트 전부=0xA6, 이전 FIL 포인터는 보관 객체다. scratch[0..1019]는 각각 0xA5000000+i, CRC CFG=0x76543210/VALUE=0x1234, bootloader_error=0x43211234, CFX 기동자료={SP=0x2345,INIT=0x6789}, CM3 기동자료={SP=0x3456,INIT=0x789A}, CFX/CM3 flag=0x67/0x89로 둔다. 후자의 플래그는 정상 논리값이 아닌 복원 확인용 표식이다.

미지원 반환을 포함한 첫 실행 후, RAM 지원 ID는 release 후 재실행 후에도 위 포인터/값/전체 배열이 모두 같아야 한다. 미지원 ID는 재실행 비교 대상이 아니다. NVMInit/read/reinit, trim, delay, timer, DIO 설정, GPIO 쓰기, 일반 파일 read/open handle, app open, 이미지 전송, 코어 기동, reset 경계 횟수는 모두 0이어야 한다. RAM 전용 callbacks의 read/write 횟수는 각 사례가 별도로 비교하며 이 금지 횟수와 구분한다. RAM_ONLY 임시 루트에는 실제 파일이 없어야 한다. 이 검사는 PC에서 수행된 격리/복원 검사다.'''
    if unit==11:
        base+='''

### BOOT 판정기 자체의 의도적 로그 변조 5종

다음 검사는 `check_oracle_guards`가 구현한 BOOT 전용 판정기 검증이다. 정상 입력은 아래의 알려진 제품 실패 한 건이다. expected=-1은 실패 전파 계약으로 고정한다.

```text
STATUS 11 0 5 1 0
ACTUAL -999 11 108 1 0
EXPECTED -999 11 108 1 -1
```

| 변조 key | 입력 변경 | 판정기 기대 동작 |
|---|---|---|
| unpaired_record | 마지막 EXPECTED 행 전체 삭제 | 실제/기대 행 짝 불일치로 AssertionError |
| wrong_length | EXPECTED 행의 마지막 -1만 제거 | 길이/metadata 불일치로 AssertionError |
| duplicate_case | STATUS count를 5→10으로 하고 같은 ACTUAL/EXPECTED 두 행 추가 | 동일 case=108 중복으로 AssertionError |
| known_failure_disappeared | EXPECTED 마지막 값을 -1→0으로 변조 | 알려진 실패가 사라졌으므로 AssertionError |
| known_failure_changed | 원 문자열의 ` 1 0` 뒤 LF를 ` 1 2` 뒤 LF로 교체 | 실제값/상태 문자열 변경으로 알려진 실패 집합 불일치, AssertionError |

각 변조의 rejected=true가 기대값이다. 변조를 받아들여 통과시키면 판정기 검사 자체가 실패한다. 이 거부 검사는 정상 제품 동작 PASS가 아니며 로그 손상/실패 숨기기 방지를 확인한다. Python `-O`는 assertion을 제거하므로 check 진입 시 RuntimeError로 거부하도록 구현되어 있다. 저장된 실행은 -O 거부를 별도 subprocess로 시험했다는 증거는 없으며 해당 경로는 분석으로 구분한다. 함수 내부에는 행 쌍/중복/길이/count 일치와 known failure의 정확한 집합 비교가 구현돼 있다.'''
    return base


def link(path: str, label: str) -> str:
    relative=Path(os.path.relpath(ROOT/path,OUT)).as_posix()
    return f'[{label}]({quote(relative, safe="/._-가-힣")})'


def format_value(value: int) -> str:
    if value < -1 or value > 255:
        return f'{value} (0x{value & 0xffffffff:08X})'
    return str(value)


def render(unit: int, title: str, records: dict, coverage: dict) -> str:
    cases=sorted(CASES[unit],key=lambda c:c['number'])
    has_ram=any(c['ram'] for c in cases)
    lines=[f'# BOOT-{unit:03d} | {title}',
           f'유닛 ID: SU-ID-BOOT-{unit:03d} / 시험 ID: SW-FW-UnitTest-BOOT-ID-{unit}',
           INTRO[unit], '## 1. 실행 범위와 비교 방법',
           '이 문서의 예상 결과는 시험 코드의 고정 상수/독립 판정식에서 정리했다. 실제값을 예상값에 복사하지 않았다. 모든 배열 인덱스는 0부터다. 정수/비트값/문자열/횟수는 정확히 일치해야 하며 허용오차는 0이다. 주소는 특별한 표기가 없으면 바이트 주소다. 물리 시간/전압/빛의 허용오차는 이 자동 시험에서 정의하지 않았다.',
           f'현재 coverage의 실행 구분은 HOST={coverage["host"]}, TARGET={coverage["target"]}다. 이 문서 작성 중 장치 시험/다운로드/기존 제품 시험 재실행은 하지 않았다. 아래 PC 관찰은 보관된 source-preserving-tests의 O0/O2 기록이다. RAM_ONLY도 PC에서 실행한 RAM 경계 모형이며 실장치 결과가 아니다.',
           '시험 빌드에서 TDC_UNIT_TEST=1은 시험 코드 포함, TDC_UNIT_TEST_HOST=1은 PC 경계 사용, TDC_UNIT_TEST_RAM_ONLY=1은 RAM 전용 사례만 선택한다. 이 매크로들은 정의 여부로 분기한다. 정상 빌드에서는 시험 코드를 실행하지 않는다. PC 시험은 매 ID마다 새 임시 파일 루트를 사용한다. RAM 사례는 읽기 위치/길이/오류/카운터를 본문 값으로 설정하고 기존 hook/key/CRC 상태를 보관한 후 복원한다.',
           '## 2. 입력 벡터와 사전 기대 결과']
    if unit in ANALYSIS:
        for i,(name,procedure,expectation,observation) in enumerate(ANALYSIS[unit],1):
            lines += [f'### 분석-{i}: {name}',f'입력/확인 절차: {procedure}',f'예상 동작/현재 계약: {expectation}',f'관찰과 판정: {observation}']
    for case in cases:
        key=(unit,case['number']); observed=records[key]['actual']
        assert records[key]['expected']==case['expected'],('expected mismatch',key,case['expected'],records[key]['expected'])
        mode='PC HOST 및 RAM_ONLY 공통 입력' if case['ram'] else 'PC HOST 입력 (모의 I/O/임시 파일)'
        lines += [f'### {case["key"]}: {case["title"]}',mode,f'초기화/입력/동작: {case["procedure"]}',
                  '| payload 인덱스 | 의미/단위 | 사전 기대값 | 기존 PC 관찰 O0/O2 |', '|---|---|---|---|']
        for i,(name,expected,actual) in enumerate(zip(case['fields'],case['expected'],observed)):
            lines.append(f'| {i} | {name} | {format_value(expected)} | {format_value(actual)} |')
        if case['behavior']: lines.append(case['behavior'])
        if observed != case['expected']:
            lines.append('판정: 알려진 제품 불일치(FAIL). PC 두 최적화에서 같은 실패가 관찰됐다. checker 종료 코드 0은 이 불일치의 정확한 재현을 허용한 것이며 제품 통과가 아니다.')
        elif case['characterization']:
            lines.append('판정: 정의된 특성값과 PC 관찰이 일치한다. 표에서 설명한 한계를 포함하며 전체 유닛/물리 동작의 PASS를 뜻하지 않는다.')
        else:
            lines.append('판정: 이 payload의 사전 기대값과 기존 PC 관찰이 일치한다. 실장치/전체 유닛의 합격을 뜻하지 않는다.')
        if case['ram']:
            prefix=f'[UT-BOOT-{unit:03d}] C{case["number"]}'
            lines.append(f'구현된 BOOT report의 통과 가정 요약(예상, 실장치 관찰 아님): `{prefix} PASS N={len(case["expected"])} F=0`. 행 끝은 CRLF다. N은 메타데이터 4워드를 제외한 payload 비교 원소 수이고 F는 불일치 원소 수다.')
            if observed != case['expected']:
                mismatch=sum(a!=e for a,e in zip(observed,case['expected']))
                lines.append(f'현재 보관 PC 값이 동일하게 report에 입력된다면 예상 요약은 `{prefix} FAIL N={len(case["expected"])} F={mismatch}`다. 이 역시 report 출력 규칙으로 계산한 예시이며 실제 RTT 수집 결과가 아니다. 앞의 PASS 가정과 현재 실패를 혼동하지 않는다.')
    extra=binary_details(unit)
    if extra: lines.append(extra)
    lines += ['## 3. 오류 주입/대기/미지원/복원 검증',boot_meta(unit,has_ram),
              '## 4. 관찰 방법과 로그',
              '값은 PC stdout의 ACTUAL/EXPECTED 또는 시험 결과 배열에서 읽는다. payload 인덱스 0은 레코드 시작의 -999, unit, case, length 네 워드 다음 첫 값이다. 결과 배열에는 여러 레코드가 이어지므로 배열 전체 인덱스와 혼동하지 않는다. 아래는 구현된 PC 출력 형식의 한 기록이다. RTT를 통해 이미 관찰했다고 주장하는 예시가 아니다.']
    if cases:
        example=next((c for c in cases if records[(unit,c['number'])]['actual']!=c['expected']),cases[0])
        metadata=f'-999 {unit} {example["number"]} {len(example["expected"])}'
        lines.append('```text\nACTUAL '+metadata+' '+' '.join(map(str,records[(unit,example['number'])]['actual']))+'\nEXPECTED '+metadata+' '+' '.join(map(str,example['expected']))+'\n```')
    else:
        lines.append(f'```text\nSTATUS {unit} -2 0 0 0\n```\n위 순서는 unit, ret, count, done, overflow다. 실제값/기대값 payload 행은 없다.')
    observer={
        4:'LED observer는 매 1 ms 모의 진행 시 values[0..3]={elapsed_ms, phase, duty_g, duty_b}를 전달한다. elapsed_ms 단위는 ms, phase는 0=IDLE/1=FADE_IN/2=PEAK/3=FADE_OUT/4=OFF/5=DONE, duty는 100단계 PWM의 켜짐 단계 수다. C5는 콜백 횟수만 비교하며 phase/duty 각각의 독립 기대값을 비교하지 않는다. 타깃 observer 데이터만으로 밝기/색/주기 PASS를 판정하지 않는다.',
        15:'전송 observer의 values[0..3]은 {구획 type, 목적지 바이트 주소, mode, 전송 count}다. PC C101~C105에서 각각 비교한 값과 count 단위는 §2에 있다. 타깃 observer의 기록 자체는 실제 메모리 내용의 일치를 판정하지 않는다.',
        16:'잠금 observer의 values[0..2]는 {구획 type, 목적지 바이트 주소, lock_value}다. lock_value는 PRAM bank별 잠금 비트마스크이며 이 입력에서는 CFX PRAM0의 1이다. 일반적인 0/1 논리값으로 다른 bank에 확대하지 않는다. PC C2의 레지스터 쓰기 추적과 별개로 observer 자체의 각 값에 대한 독립 비교는 없다. 타깃 기록만으로 접근 차단 성공을 판정하지 않는다.',
        18:'서비스 LED observer values[0]은 색 비트마스크(비트0=R, 1=G, 2=B; 각 비트 0=꺼짐, 1=켜짐)이고 GPIO 출력은 반전된 active-low다. 재기동 observer values[0]=1은 reset 요청 직전 도달을 뜻한다. PC 사례는 reset 호출 수/GPIO 쓰기 증가/코어 기동 없음만 비교하고 observer의 색 값이나 물리 reset 완료는 비교하지 않는다.',
    }
    if unit in observer: lines.append(observer[unit])
    if has_ram:
        lines.append('RAM 지원 사례의 요약은 완료된 결과가 있고 overflow=0이며 전경에서 report_begin 후 report_poll을 끝까지 소비했을 때의 구현 출력 규칙이다. 출력은 채널 0의 SEGGER_RTT_Write를 사용하며 ISR 안에서 출력하지 않는다. 실기 적용 시 시험 코드와 RTT 초기화, 전경 단독 실행 및 관련 파일/NVM/CRC 작업의 동시 접근 방지가 전제다. PC-only 사례나 미지원 ID에 타깃 PASS 요약을 대응시키지 않는다.')
    lines += ['별도 기대값 없이 observer에 저장한 값은 관찰 및 판정 보류다. 이 문서는 없는 RTT PASS 문자열이나 실제 reset 완료 메시지를 만들지 않는다.',
              '## 5. 알려진 불일치와 잔여 확인',*['- '+s for s in LIMITS[unit]],
              '## 6. 근거 소스와 보관 결과']
    refs=list(dict.fromkeys([RUNNER,HOST,'tests/unit/tdc_ut_boot_check.py',COVERAGE,*SOURCES[unit],
        'src/0__bootloader/include/bootloader/bootloader_application_file.h','src/0__bootloader/include/bootloader/bootloader_internal.h',
        QA.relative_to(ROOT).as_posix(),(TASK/'qa/source-preserving-tests/suite/tdc_ut_boot_check.log').relative_to(ROOT).as_posix(),
        'tmp/unit-tests/boot/checks.json',(TASK/'qa/source-preserving-tests/coverage/115-units.json').relative_to(ROOT).as_posix()]))
    if has_ram: refs+=['src/0__bootloader/source/test/tdc_ut_boot_report.c','src/test/tdc_ut_log.c']
    lines.extend('- '+link(p,Path(p).name) for p in refs)
    lines.append('정식 명칭은 115유닛 대응의 name을 사용했다. 입력과 기대값의 구체적 근거 함수는 위 시험 소스의 '+', '.join(sorted({c['source'].split(':')[-1] for c in cases}) or ['해당 없음(분석 전용)'])+'이며, 배열/판정 비교는 tdc_ut_boot_check.py의 parse_output/verify_failures/run_units를 대조했다.')
    document='\n\n'.join(lines)+'\n'
    return re.sub(r'(?m)(\|[^\n]*\|)\n\n(?=\|)(?!\|[^\n]*\|\n(?:\n)?\|---)',r'\1\n',document)


def validate_evidence(evidence: dict, expected_map: dict) -> list[dict]:
    known={(7,6):([0],[-1]),(7,17):([0,0,0],[-9,1,0]),(11,6):([0],[-1]),
           (11,108):([0],[-1]),(11,109):([0],[-1]),(11,110):([1],[-1])}
    checks=[]
    runs=evidence['runs']
    assert [(r['mode'],r['optimization']) for r in runs]==[('HOST','-O0'),('HOST','-O2'),('RAM_ONLY','-O0'),('RAM_ONLY','-O2')]
    for run in runs:
        ram=run['mode']=='RAM_ONLY'
        wanted={key:c for key,c in expected_map.items() if not ram or c['ram']}
        got={(r['unit'],r['case']):r for r in run['records']}
        assert len(got)==len(run['records'])==len(wanted)
        assert got.keys()==wanted.keys()
        assert all(got[k]['expected']==c['expected'] for k,c in wanted.items())
        failures={k:(r['actual'],r['expected']) for k,r in got.items() if r['actual']!=r['expected']}
        assert failures=={k:v for k,v in known.items() if k in wanted}
        assert all(r['passed']==(r['actual']==r['expected']) for r in got.values())
        assert len(run['statuses'])==20
        for status in run['statuses']:
            n=status['unit']
            words=sum(4+len(c['expected']) for (u,_),c in wanted.items() if u==n)
            assert status==dict(unit=n,ret=0 if words else -2,count=words,done=1 if words else 0,overflow=0)
        checks.append(dict(key=f'{run["mode"]}/{run["optimization"]}',status='검증됨',records=len(got),known_failures=len(failures),
                           expected_origin='C 시험의 상수/독립식으로 별도 작성 후 저장된 expected와 대조',actual_used_for_expected=False))
    assert runs[0]['records']==runs[1]['records'] and runs[2]['records']==runs[3]['records']
    assert len(expected_map)==198
    assert len([c for c in expected_map.values() if c['ram']])==80
    assert len(evidence['oracle_guards'])==5 and all(g['rejected'] for g in evidence['oracle_guards'])
    checks.extend([
        dict(key='expected_provenance',status='검증됨',detail='198개 기대 payload를 C 상수/독립식으로 정의. actual은 관찰 렌더와 불일치 검증에만 사용. expected 대입 경로에 actual 없음.'),
        dict(key='boot_oracle_mutations',status='기존 증거 확인',cases=evidence['oracle_guards']),
        dict(key='boot_pending_restore_repeat',status='기존 실행 종료코드와 소스 대조',detail='전 128개 배열 BUSY 불변, RAM 재실행, nonzero 이전 상태/1020 scratch 복원, 금지 경계 0회, RAM 파일 미생성. 본문 각 유닛 §3.'),
        dict(key='unsupported_ids',status='기존 증거 확인',detail='HOST 1/3/5/6, RAM_ONLY는 8/9/10/11/12/13/14/19/20 이외 1..20 ID. -2,count0,done0,overflow0.'),
        dict(key='python_optimized_checker_rejection',status='분석',detail='check()의 __debug__ 거부 구현 확인. 별도 -O 실행 증거 없음.'),
        dict(key='target_execution',status='미실행',detail='문서 작성/검증만 수행; 실장치 결과와 전체 baseline 동일성은 이 검증의 증거 범위가 아님.'),
        dict(key='ram_report_summaries',status='구현 형식과 대조',records=80,detail='RAM 지원 payload별 C/N 통과 가정 요약. 현재 불일치 3건은 FAIL 예상 요약 별도. 모두 실제 RTT 관찰이 아닌 출력 규칙 예측.'),
        dict(key='qcc_insertion',status='해당 없음',detail='BOOT 신규 문서에는 QCC 내부 자료 추가 위치 없음. 기존 QCC 관리 목록은 수정하지 않음.')])
    return checks


def build() -> tuple[dict[Path,str],dict]:
    define_cases(); define_text()
    evidence=json.loads(QA.read_text(encoding='utf-8'))
    unit_inventory=json.loads((TASK/'qa/source-preserving-tests/coverage/115-units.json').read_text(encoding='utf-8'))
    names={entry['unit']:entry['name'] for entry in unit_inventory['units'] if entry['core']=='BOOT'}
    coverage=json.loads((ROOT/COVERAGE).read_text(encoding='utf-8'))
    scope={entry['unit']:entry for entry in coverage['coverage']}
    expected_map={(n,c['number']):c for n,rows in CASES.items() for c in rows}
    assert sum(map(len,CASES.values()))==len(expected_map)
    checks=validate_evidence(evidence,expected_map)
    observed={(r['unit'],r['case']):r for r in evidence['runs'][0]['records']}
    assert all(scope[n]['records']==len(CASES[n]) for n in range(1,21))
    outputs={}; units=[]
    for n in range(1,21):
        path=OUT/f'BOOT-{n:03d}.md'
        outputs[path]=render(n,names[n],observed,scope[n])
        records=[dict(key=c['key'],source=c['source'],
                      expected_fields=[dict(index=i,name=name,value=c['expected'][i],tolerance=0) for i,name in enumerate(c['fields'])],
                      mode=['PC_HOST','PC_RAM_ONLY'] if c['ram'] else ['PC_HOST'],
                      title=c['title'],inputs_and_steps=c['procedure'],
                      characterization=c['characterization'],known_failure=(n,c['number']) in {(7,6),(7,17),(11,6),(11,108),(11,109),(11,110)},
                      target_report_pass_if_all_match=f'[UT-BOOT-{n:03d}] C{c["number"]} PASS N={len(c["expected"])} F=0' if c['ram'] else None)
                 for c in sorted(CASES[n],key=lambda x:x['number'])]
        records.extend(dict(key=f'분석-{i}',source=';'.join(SOURCES[n]),expected_fields=[dict(name='예상 동작/현재 계약',value=item[2])],mode=['분석'],title=item[0],inputs_and_steps=item[1])
                       for i,item in enumerate(ANALYSIS.get(n,[]),1))
        units.append(dict(id=f'BOOT-{n:03d}',title=names[n],file=path.relative_to(ROOT).as_posix(),cases=records,limitations=LIMITS[n],
                          payload_records=len(CASES[n]),ram_payload_records=sum(c['ram'] for c in CASES[n])))
    checks.append(dict(key='document_inventory',status='검증됨',files=20,payloads=198,ram_subset=80,analysis_scenarios=sum(map(len,ANALYSIS.values())),
                       detail='O0/O2 및 RAM 반복의 동일 벡터는 중복 작성하지 않음. 별도 C 번호/초기화 차이는 모두 유지.'))
    manifest=dict(role='boot',units=units,checks=checks)
    outputs[INDEX]=json.dumps(manifest,ensure_ascii=False,indent=2)+'\n'
    return outputs,manifest


def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--patch',action='store_true')
    parser.add_argument('--check',action='store_true')
    parser.add_argument('--units',help='출력할 ID 번호를 쉼표로 지정한다. 파일 변경은 여전히 patch 적용자가 수행한다.')
    parser.add_argument('--manifest-only',action='store_true')
    args=parser.parse_args()
    outputs,manifest=build()
    if args.patch:
        if args.units:
            selected={int(n) for n in args.units.split(',')}
            outputs={p:c for p,c in outputs.items() if p.suffix=='.md' and int(p.stem.split('-')[1]) in selected}
        if args.manifest_only:
            outputs={INDEX:outputs[INDEX]}
        print('*** Begin Patch')
        for path,content in outputs.items():
            if path.exists():
                old=path.read_text(encoding='utf-8')
                if old==content: continue
                print('*** Update File: '+path.as_posix())
                diff=list(difflib.unified_diff(old.splitlines(),content.splitlines(),n=3))
                for line in diff[2:]: print('@@' if line.startswith('@@') else line)
            else:
                print('*** Add File: '+path.as_posix())
                for line in content.splitlines(): print('+'+line)
        print('*** End Patch')
    elif args.check:
        links=0
        summaries=0
        for path,content in outputs.items():
            assert path.read_text(encoding='utf-8')==content, ('render differs',path)
            if path.suffix=='.md':
                for target in re.findall(r'\]\(([^)]+)\)',content):
                    assert (path.parent/unquote(target)).resolve().exists(),(path,target)
                    links+=1
                assert content.count('\n# ')==0
                assert content.count('```')%2==0
                assert '\ufffd' not in content
                unit=int(path.stem.split('-')[1])
                expected_logs=[f'[UT-BOOT-{unit:03d}] C{c["number"]} PASS N={len(c["expected"])} F=0' for c in CASES[unit] if c['ram']]
                assert all(content.count(s)==1 for s in expected_logs)
                assert len(re.findall(r'\[UT-BOOT-\d{3}\] C\d+ PASS N=\d+ F=0',content))==len(expected_logs)
                assert '주 작업' not in content and 'main 담당' not in content
                summaries+=len(expected_logs)
        assert summaries==80
        # 기대값 정의가 QA.actual이나 파일 읽기를 요구하지 않는지 별도 검사한다.
        from unittest.mock import patch
        original={(n,c['number']):c['expected'] for n,rows in CASES.items() for c in rows}
        for rows in CASES.values(): rows.clear()
        with patch.object(Path,'read_text',side_effect=AssertionError('expected definition attempted file I/O')):
            define_cases()
        assert original=={(n,c['number']):c['expected'] for n,rows in CASES.items() for c in rows}
        # 기존 checker의 변조 가드를 메모리에서만 호출한다. check()는 실행하지 않는다.
        import runpy
        checker=runpy.run_path(str(ROOT/'tests/unit/tdc_ut_boot_check.py'),run_name='boot_doc_review')
        checker['check_oracle_guards']()
        assert len(checker['EVIDENCE']['oracle_guards'])==5
        print(json.dumps(dict(role='boot',documents=20,manifest=str(INDEX.relative_to(ROOT)),payloads=198,ram_subset=80,
                              known_failures_host=6,known_failures_ram=3,source_expected_matches=True,
                              actual_used_for_expected=False,links_checked=links,oracle_mutations_rejected=5,
                              ram_pass_summary_count=summaries,expected_definition_without_evidence_io=True,
                              device_executed=False,baseline_checked=False),ensure_ascii=False))
    else:
        print(json.dumps(manifest['checks'],ensure_ascii=False,indent=2))


if __name__=='__main__':
    main()
