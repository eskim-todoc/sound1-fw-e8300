"""담당 14유닛 문서/검증목록을 구성하고 검증한다. 제품/시험 파일 쓰기 없음.

--emit은 apply_patch에 전달할 파일 내용을 stdout JSON으로만 반환한다.
--check는 저장 문서, 모든 사전 기대값, 입력/벡터 대응과 링크를 읽기 전용 검사한다.
--logs-export는 별도 전문 포매터용 배열을 stdout JSON으로만 내보낸다.
--logs-check는 해당 export 파일을 독립 기대값과 읽기 전용 대조한다.
실제 출력(actual)은 관찰 표에만 사용하며 기대값 계산에는 사용하지 않는다.
"""
from __future__ import annotations

import argparse
import ast
import json
import math
import os
from pathlib import Path
import re
import sys
from urllib.parse import quote, unquote

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[5]
TASK = Path("docs/tasks/인허가/20260929_1755_DHF-펌웨어-설계출력")
OUT = Path("docs/참고/인허가/사운드1/DHF/04_설계 검증/SW/펌웨어/유닛 시험 예상 입출력")
RUNNER = "src/1__cfx/test/tdc_ut_io_cfx.c"
EXTRA = "tests/unit/tdc_ut_cfx_io_cases.c"
REF = "tests/unit/tdc_ut_hear_reference.json"
EVIDENCE = TASK / "qa/source-preserving-tests/cfx-io/checks.json"
MANIFEST = TASK / "qa/expected-io/cfx-io-hear.json"
LOG_EXPORT = TASK / "qa/expected-io/cfx-io-hear-logs.json"
REPORT = "src/2__cm3/Cortex-M3-src/test/tdc_ut_report.c"
LOG = "src/test/tdc_ut_log.c"
NOP = 0x25555
ORDER = [0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15]
TITLES = {7: "마이크 블록 수집", 10: "내부기 연결 상태 반영", 11: "맵 및 볼륨 변경 처리",
          12: "공유 맵 데이터 복사", 15: "I2S 수신 버퍼 관리", 19: "I2S 시작과 중단의 완만한 전환",
          26: "주파수 분석 입력 갱신", 27: "FFT 대역 인덱스 읽기", 33: "CIS 전략과 전략 선택",
          36: "자극 패킷 헤더 구성", 37: "NofM 패킷 구성과 분할 출력", 40: "지정 명령 PCM 출력"}
PRODUCTS = {
    7: ["systemControl/main.c"], 10: ["systemControl/system_control.c", "signalProcessing/definitionsForAlgorithm.h"],
    11: ["systemControl/system_control.c", "signalProcessing/nonlinearMapping.c", "lib_cfx/lib_audio_in.c", "environment/shared_memory.h"],
    12: ["systemControl/system_control.c", "environment/shared_memory.h"],
    15: ["lib_cfx/lib_i2s.c", "lib_cfx/lib_i2s.h"], 19: ["lib_cfx/lib_i2s.c", "lib_cfx/lib_i2s.h"],
    26: ["signalProcessing/FrequencyAnalysis.c"], 27: ["signalProcessing/FrequencyAnalysis.c"],
    33: ["signalProcessing/stimulationStrategy.c", "signalProcessing/FrequencyAnalysis.c", "signalProcessing/nonlinearMapping.c"],
    36: ["signalProcessing/stimulationStrategy.c", "internalDevice/driver_PCM.h"],
    37: ["signalProcessing/stimulationStrategy.c", "internalDevice/driver_PCM.h"],
    40: ["internalDevice/driver_PCM.c", "internalDevice/driver_PCM.h"],
}


def read_json(path: str | Path) -> dict:
    return json.loads((ROOT / path).read_text(encoding="utf-8-sig"))


def packet(phase: int, electrode: int, amplitude: int) -> int:
    """시험의 독립 비트 배치. electrode는 0기준, amplitude는 DAC 코드다."""
    return 0x40000 | (phase << 15) | (electrode << 10) | (amplitude << 2)


def block(words: list[int], start: int, pulses: int, stride: int) -> list[int]:
    return [words[(start + i // stride) % len(words)] if i % stride == 0 and i // stride < pulses else NOP
            for i in range(24)]


def field(name: str, value: int | list[int]) -> dict:
    return {"field": name, "values": value if isinstance(value, list) else [value]}


def case(key: int | str, title: str, inputs: str, fields: list[dict], *, extra=False,
         mode="PC / 모의 IO / RAM", limitation="", source=None) -> dict:
    return {"key": str(key), "title": title, "input_and_action": inputs, "expected_fields": fields,
            "source": source or (EXTRA if extra else RUNNER), "mode": mode,
            "limitation": limitation, "extra": extra}


def payload(c: dict) -> list[int]:
    return [v for f in c["expected_fields"] for v in f["values"]]


def map_arrays() -> list[list[int]]:
    return [[i + 1 for i in range(32)], [32 - i for i in range(32)], [i + 1 for i in range(32)],
            [50 + i for i in range(32)], [150 + i for i in range(32)],
            [300 + i for i in range(32)], [600 + i for i in range(32)]]


MAP_NAMES = ["자극 전극 배정(1..32)", "기준 전극 배정(1..32)", "CIS 대역 순서(1..32)",
             "T 레벨(µA)", "C 레벨(µA)", "오디오 최소 입력(정수 코드)", "오디오 최대 입력(정수 코드)"]
MAP_INPUT = """공유 맵의 전략은 별도 지정값, 첫 위상은 1(양극 선행), 자극 모드는 en__bipolar=4,
펄스 한 위상 폭은 40 µs, 대역 수는 32, 표시 자극 채널은 12, 표시 전류는 90 µA로 설정한다.
날짜 필드 [년,월,일,시,분,초]의 입력 6워드는 10+i, 기존 목적지는 70+i(i=0..5)로 둔다.
날짜는 복사 구별용 코드이며 달력 유효성 시험이 아니다.
각 대역 i=0..31에 자극 전극=i+1, 기준 전극=32-i, CIS 순서=i+1,
T=50+i µA, C=150+i µA, 오디오 최소=300+i, 최대=600+i를 모두 설정한다.
전극 번호와 CIS 순서 값은 1부터 시작하고 배열 인덱스는 0부터 시작한다."""


def cfx_cases() -> dict[int, list[dict]]:
    units: dict[int, list[dict]] = {u: [] for u in TITLES}
    units[7] = [case(1, "두 마이크의 연속 두 블록과 이력 이동",
        "m=0은 DMIC1(EZ), m=1은 DMIC2(QCC라고 명명된 마이크)이다. QCC I2S 입력과는 다르다. "
        "표본 i=0..15에 첫 블록 A[m][i]=1000*m+i를 RAM FIFO에 넣고 tdc_copy_DMIC_buffers()를 호출한다. "
        "다음 블록 B[m][i]=100+1000*m+i를 넣고 다시 호출한다. 호출 전 이력은 어떤 값이어도 두 호출 후 두 슬롯이 모두 결정된다.",
        [field(f"마이크 {m}, 이력 b={b}({'최신 B' if b == 0 else '직전 A'}), 표본 i=0..15",
               [(100 if b == 0 else 0) + 1000*m+i for i in range(16)]) for m in range(2) for b in range(2)])]
    units[10] = [case(1, "내부기 연결 번호 0에서 1로 변경",
        "맵 변경 요청과 CM3 계산 완료를 각각 0(요청 없음)으로 두고, 공유 오디오/자극 볼륨을 각각 직전 값과 같게 한다. "
        "isMappingProgramConneted=0(매핑 앱 미연결), m_previous_isd=0(내부기 없음), connected_ISD_num=1(내부기 1)로 두고 LB_Normal_PowerMode()를 호출한다.",
        [field("[현재 내부기 번호, 직전 내부기 번호, 사용자 설정 반영 알림]", [1,1,1])]),
        case(2, "연결 해제 시 이전 AGC 게인과 연결 검사 카운터 초기화",
        "case 1 직후 connected_ISD_num=0(미연결), m_prev_agc_10db_gain_Q8_16=999로 설정하고 다시 호출한다. "
        "사용자 설정 알림은 1=연결 번호 변경을 CFX가 확인함, 0=연결 해제를 확인함이다. "
        "연결 검사 주기 상수는 300 ms이며 카운터는 그 음수 값에서 다시 시작한다.",
        [field("[현재 내부기, 직전 내부기, 설정 알림, 이전 AGC 게인 Q8.16, 연결 카운터(ms 단위 상태값)]", [0,0,0,0,-300])])]
    base_volume = "32개 채널 i=0..31 모두 T=50, C=150(DAC 코드), xmin=0, xmax=2(오디오 정수 코드)로 둔다. " \
        "power override=1은 기존 시험 경계에서 x=1/2만 각각 x*65536(Q8.16)으로 고정한다. x=0은 실제 0 경로다. " \
        "이 조건은 SDK 로그/거듭제곱 정확도 시험이 아니다. A/B는 정수 저장값을 256으로 나누는 Q16.8 계수다. "
    volumes = [(1,1),(4,10),(4,1),(1,1),(1,1)]
    volume_titles = ["최저 자극/오디오 볼륨 동시 변경", "최대 자극/오디오 볼륨 동시 변경", "오디오 볼륨만 10에서 1로 변경", "자극 볼륨만 4에서 1로 변경", "같은 볼륨 재요청"]
    for v,(stim,audio) in enumerate(volumes):
        cnew = 119 if stim == 1 else 150
        units[11].append(case(v+1, volume_titles[v], base_volume +
            ("직전 두 볼륨을 -1(미반영)로 초기화한다. " if v<2 else "앞 case의 결과를 이어 사용한다. ") +
            f"(자극 볼륨, 오디오 볼륨)=({stim},{audio})를 주고 Normal_PowerMode_event_both_stimulation_audio_volumeChange()를 1회 호출한다. "
            "자극 볼륨의 유효 단계는 1..4, 오디오 볼륨은 1..10이다. 변경 전 모의 PCM 24슬롯은 -777로 초기화한다.",
            [field("채널 i=0..31 Cnew(DAC 코드)", [cnew]*32), field("채널 i=0..31 T(DAC 코드)", [50]*32),
             field("채널 i=0..31 A(Q16.8 정수)", [(cnew-50)*128]*32), field("채널 i=0..31 B(Q16.8 정수)", [12800]*32),
             field("[직전 자극 볼륨, 직전 오디오 볼륨, 내부 오디오 인덱스, PCM 쓰기 횟수]", [stim,audio,audio-1,0 if v==4 else 24])]))
    for v,(stim,audio) in enumerate([(0,1),(-1,1),(1,0),(1,-1)]):
        units[11].append(case(20+v, f"비양수 볼륨 ({stim},{audio}) 거부",
            f"case 5 결과(T=50/Cnew=119/A=8832/B=12800)를 유지하고 ({stim},{audio})를 요청한다. "
            "PCM 쓰기 계수를 0으로 초기화하고 같은 이벤트 함수를 호출한다. 전체 32채널과 직전 볼륨은 그대로여야 한다.",
            [field("[32채널 불변 여부(1=모두 불변), 직전 자극 볼륨, 직전 오디오 볼륨, 내부 인덱스, PCM 쓰기 횟수, overflow(0=없음)]", [1,1,1,0,0,0])]))
    for v,stim in enumerate([2,3]):
        cnew = [129,139][v]
        units[11].append(case(30+v, f"거부 후 자극 볼륨 {stim}으로 정상 복귀",
            f"위 거부 사례에 이어 ({stim},1)을 요청한다. T=50/C=150/xmin=0/xmax=2와 power 고정 조건을 유지한다. "
            "비율 Q2.22는 단계 2=3355443, 단계 3=3774873이다. Cnew=50+((100*비율)>>22), A=(Cnew-50)*128, B=12800이다. "
            "전체 32채널에 같은 계산을 적용하지만 이 복귀 레코드는 채널 0/15/31만 직접 비교한다.",
            [field("채널 0/15/31 순서로 각 [Cnew, A, B]", [cnew,(cnew-50)*128,12800]*3),
             field("[직전 자극 볼륨, 직전 오디오 볼륨, 내부 인덱스, PCM 쓰기 횟수]", [stim,1,0,24])]))
    for no_date in [False,True]:
        units[12].append(case(2 if no_date else 1, "날짜를 제외한 복사" if no_date else "날짜를 포함한 전체 복사",
            MAP_INPUT + " 전략=1(CIS)로 둔다. 각 case 시작마다 입력/날짜 목적지를 다시 위 값으로 설정한다. " +
            ("copy_MappingData_without_mappingDate()를 호출한다." if no_date else "copy_MappingData()를 호출한다."),
            [field("날짜 6워드 [년,월,일,시,분,초]", [(70 if no_date else 10)+i for i in range(6)]),
             field("[전략(CIS=1), 첫 위상(양극 선행=1), 모드(bipolar=4), 폭(µs), 대역 수, 표시 채널, 표시 전류(µA)]", [1,1,4,40,32,12,90])] +
            [field(name+", i=0..31", arr) for name,arr in zip(MAP_NAMES,map_arrays())]))
    units[15] = [case(1, "I2S 16표본 복사와 완료 표시",
        "입력 s_tdc_ut_input[i]=100+i(i=0..15), 출력 버퍼[i]=55로 둔다. 쓰기 위치=0, 누적 복사 수=0, "
        "버퍼 상태 INIT=0(아직 초기 충전 중), interrupt_flag=1(새 FIFO 블록 있음), copy_done=0(완료 미게시)로 초기화한다. "
        "lib_i2s_copy_data_from_fifo(input)을 1회 호출한다. copy_done=1은 새 복사가 완료되어 소비자가 확인할 수 있음을 뜻한다.",
        [field("수신 링버퍼[0][i], i=0..15", list(range(100,116))), field("[다음 쓰기 위치, 누적 복사 수, interrupt_flag, copy_done]", [1,1,0,1])]),
        case(2, "완료 표시와 출력 버퍼 지우기",
        "case 1 뒤 lib_i2s_clear_buffer_copy_done(), lib_i2s_clear_data()를 순서대로 호출한다. "
        "clear_data는 출력 버퍼 16워드를 지우며 링버퍼 8블록 전체 삭제로 해석하지 않는다.",
        [field("copy_done(0=완료 알림 해제)",0),field("출력 버퍼 i=0..15",[0]*16)])]
    fades = [[0,1023,2047,3071,4095,5119,6143,7167,8191,9215,10239,11263,12287,13311,14335,15359],
             [49151,50175,51199,52223,53247,54271,55295,56319,57343,58367,59391,60415,61439,62463,63487,65536],
             [65536,63487,62463,61439,60415,59391,58367,57343,56319,55295,54271,53247,52223,51199,50175,49151],
             [15359,14335,13311,12287,11263,10239,9215,8191,7167,6143,5119,4095,3071,2047,1023,0], [0]*16]
    for v,state in enumerate([0,3,6,9,10]):
        units[19].append(case(v+1, ["fade-in 첫 단계", "fade-in 마지막 단계", "fade-out 첫 단계", "fade-out 마지막 단계", "fade-out 종료 후 0 버퍼"][v],
            f"각 case마다 8개 링블록 b=0..7의 표본 i=0..15 전체를 65536으로 다시 채우고 읽기 위치=0, sub_state={state}로 둔다. "
            + ("I2S_get_buffer_with_fade_in_process()" if v<2 else "I2S_get_buffer_with_fade_out_process()") +
            "를 1회 호출한다. 현재 빌드는 fade 단계 수=4이다. sub_state 0..3은 fade-in, 6..9는 fade-out, 10은 현재 4단계 구성의 종료 값이다. "
            "입력/출력은 오디오 정수 코드, 계수는 Q16으로 65536=1.0이다. 곱셈 후 >>16으로 절삭한다. "
            "비교 순서는 반환 배열의 output[15-i]이므로 아래 표 인덱스 i와 실제 메모리 주소 방향이 반대다.",
            [field("비교 i=0..15에 대한 `output[15-i]` (정수 코드)",fades[v])]))
    units[26] = [case(1,"AGC 블록의 역순 FFT FIFO 기록",
        "m_agc_output_buffer[i]=i(i=0..15)로 채우고 FFT 쓰기 기록 횟수를 0으로 초기화한다. "
        "update_FFT_inputData()를 1회 호출한다. 각 값은 오디오 정수 코드이며 실제 FFT 연산은 이 함수에 없다.",
        [field("FFT FIFO 쓰기 횟수",16),field("시간순 쓰기 j=0..15",list(range(15,-1,-1)))])]
    for v,count in enumerate([31,32]):
        units[27].append(case(v+1,"대역 수 불일치로 기존 표 유지" if v==0 else "대역 수 일치로 256개 표 복사",
            "맵 대역 수=32, FS 모의 입력 bin[i]=i%32(i=0..255), 목적지 g_pass_bin_index[i]=-1로 준비한다. "
            + ("" if v==0 else "case 1 직후 이어서 ") + f"FS 대역 수={count}를 주고 read_FFT_PassBin_index()를 호출한다. "
            "배열 인덱스 i는 FFT bin 번호, 값 -1은 미사용, 값 0..31은 목적 주파수 대역 번호이다. 수치 단위는 인덱스다.",
            [field("g_pass_bin_index[i], i=0..255",[-1]*256 if v==0 else [i%32 for i in range(256)])]))
    reverse_words = [packet(0,(i*7)%32,[0,1,127,255][(31-i)%4]) for i in range(32)]
    reverse_in = "대역=32, 첫 위상=0(음극 선행), 묵음 표시=0(이 패킷 입력에서 비활성), 채널당 프레임=2, " \
        "블록당 전송 가능 채널=12, 최초 전송 인덱스=0으로 둔다. 모든 i=0..31에 CIS 순서=32-i, " \
        "자극 전극 배정=32-i, 물리 전극표[i]=(7*i)%32, 진폭[i]=[0,1,127,255][i%4]를 입력한다. " \
        "진폭은 0..255 DAC 코드로 전류 µA와 다르다. 패킷은 W=0x40000|(phase<<15)|(물리전극<<10)|(진폭<<2)이다. "
    for v in range(3):
        units[33].append(case(10+v,f"역순 32대역 블록 {v+1}의 패킷 32개",
            reverse_in + f"동일 입력으로 stimulationStrategy_CIS()를 연속 3회 호출하는 중 {v+1}번째이다. "
            "각 호출 직전 PCM 24슬롯은 -777(아직 쓰지 않음)로 다시 채우고 전송 인덱스는 이어간다. "
            "출력 패킷 위치 j의 입력 대역은 31-j, 배정 전극 번호는 j+1, 물리 전극은 (7*j)%32이다.",
            [field("패킷[j], j=0..31", reverse_words)]))
        units[33].append(case(v+1,f"역순 32대역 블록 {v+1}의 PCM 24슬롯",
            f"바로 위 case {10+v} 호출에서 기록한 PCM을 비교한다. 시작 패킷 위치={(v*12)%32}이다. "
            "슬롯 j=0..23 중 짝수 j는 패킷[(시작+j/2)%32], 홀수 j는 NOP=0x25555(152917)이다. "
            "PCM 슬롯 j는 쓰기 시작 포인터에서 j워드 감소한 위치다.",
            [field("PCM 슬롯 j=0..23", block(reverse_words,v*12,12,2)),field("다음 전송 인덱스",((v+1)*12)%32)]))
    for v,(bands,stride) in enumerate([(1,2),(3,1),(11,2),(12,2)]):
        expected=[packet(0,j//stride,j//stride+1) if j%stride==0 and j//stride<bands else NOP for j in range(24)]
        units[33].append(case(20+v,f"작은 대역/동일 경계: {bands}대역, {stride}프레임",
            f"대역 수={bands}, 프레임 간격={stride}, 전송 가능 채널=12, 전송 인덱스=0으로 초기화한다. "
            "첫 위상=0, 전체 i=0..31에 CIS 순서=i+1, 전극 배정=i+1, 물리표=i, 진폭=i+1로 둔다. "
            "PCM 24슬롯을 -777로 채운 후 stimulationStrategy_CIS()를 1회 호출한다. "
            "기대 배치는 유효 대역을 각각 한 번만 해당 간격으로 배치하고 나머지 슬롯을 NOP로 채우는 것이다.",
            [field("PCM 슬롯 j=0..23",expected),field("[다음 전송 인덱스, overflow]",[0,0])],
            mode="PC / 모의 IO / RAM / 비정상 상태 조합" if v==1 else "PC / 모의 IO / RAM",
            limitation="case 21의 stride=1/transferable=12는 현재 CM3의 floor(24/stride) 계산에서 나오지 않는 조합이다. 유효 설정의 제품 실패와 분리한다." if v==1 else ""))
    equal_words=[packet(0,i,100) for i in range(32)]
    for v in range(3):
        units[33].append(case(30+v,f"등진폭 100, 정순 32대역의 블록 {v+1}",
            "대역=32, 프레임 간격=2, 전송량=12, 최초 인덱스=0으로 두고, i=0..31 전체에 "
            "CIS 순서=i+1, 전극 배정=i+1, 물리표=i, 진폭=100, 첫 위상=0을 설정한다. "
            f"PCM을 -777로 다시 채우며 연속 세 호출 중 {v+1}번째 결과를 확인한다. 블록 사이에는 전송 인덱스를 초기화하지 않는다.",
            [field("패킷[i], i=0..31",equal_words),field("PCM 슬롯 j=0..23",block(equal_words,v*12,12,2)),
             field("다음 전송 인덱스",((v+1)*12)%32)]))
    for phase in [0,1]:
        words=[packet(phase,e,a) for e,a in zip([0,1,16,31],[0,1,127,255])]
        f=[field("자극 패킷 헤더",0x40000|(phase<<15))]
        for i,(e,a,w) in enumerate(zip([0,1,16,31],[0,1,127,255],words)):
            f.append(field(f"패킷 {i}: [완성 워드, type 비트19..16, 위상 비트15, 물리전극 비트14..10, 진폭 비트9..2, 하위2비트]",[w,4,phase,e,a,0]))
        units[36].append(case(phase+1,f"첫 위상 {phase}의 헤더와 네 패킷 비트 필드",
            f"firstPulsePhase={phase}({'음극' if phase==0 else '양극'} 선행)로 prepare_pcmStimulationPacketHeader()를 호출한다. "
            "대역=4, CIS 순서=[1,2,3,4], 전극 배정=[1,2,17,32], 진폭=[0,1,127,255], "
            "물리표[i]=i(i=0..31), 묵음=0, 프레임=2, 전송량=12, 전송 인덱스=0으로 두고 CIS를 호출한다. "
            "비교 대상은 구성된 패킷/비트 필드다. 이 호출의 작은 대역 PCM 출력이 맞다는 판정은 포함하지 않는다.",f))
    for phase in [0,1]:
        selected=ORDER if phase==0 else [15]+ORDER[:-1]
        words=[packet(phase,((31-b)*7)%32,[0,1,127,255][b%4]) for b in selected]
        units[37].append(case(10+phase, "기존 교차 순서 유지" if phase==0 else "마지막 한 대역을 맨 앞으로 이동",
            f"선택 대역(0기준)={ORDER}, 직전 마지막 대역={15 if phase==0 else 0}, 첫 위상={phase}(0=음극 선행/1=양극 선행)로 둔다. "
            "모든 b=0..31에 전극 배정=32-b, 진폭=[0,1,127,255][b%4], 물리표[e]=(7*e)%32를 입력한다. "
            "묵음=0으로 두고 헤더 준비 후 stimulationStrategy_NofM_Packet()을 호출한다. "
            f"기대 출력 대역 순서는 {selected}이다. 이 이동은 전체 역순 정렬이 아니다.",
            [field("패킷 j=0..15",words),field("다음 블록에서 사용할 마지막 대역",15 if phase==0 else 7)]))
        for p in [0,1]:
            units[37].append(case(phase*2+p+1,f"위상 {phase} 패킷의 {'첫' if p==0 else '둘째'} 8개 출력",
                f"case {10+phase}의 16패킷을 사용한다. 프레임 간격=3, 전송 가능 채널=8, 최초 인덱스=0으로 두고 "
                f"stimulationStrategy_NofM_PCM_Write()를 두 번 호출하는 중 {p+1}번째를 확인한다. "
                "매 호출 PCM 24슬롯은 -777로 초기화하며 인덱스는 이어간다. 함수는 먼저 NOP 24개를 쓰고 "
                "슬롯 0,3,6,9,12,15,18,21에 패킷을 덮어쓴다. 최종 24슬롯과 다음 인덱스를 비교한다.",
                [field("PCM 슬롯 j=0..23",block(words,p*8,8,3)),field("다음 전송 인덱스",8 if p==0 else 0)]))
    for v in [0,1]:
        words=[0x11,0x22,0x33]+[NOP]*21 if v==0 else [0x50000|(i<<9)|(0x80+i) for i in range(24)]
        f=[field("PCM 쓰기 횟수",24)]
        f += [field(f"쓰기 i={i}: [슬롯 offset, 입력 그대로인 워드, 읽기중 플래그, 쓰기 순간 이전 읽기 인덱스]",[i,w,1,17 if i==0 else i-1]) for i,w in enumerate(words)]
        f += [field("완료 후 [읽기중 플래그, 최종 읽기 인덱스, PCM 모드]",[0,23,4]),field("변경되지 않은 입력 PCM_specificBuffer[i], i=0..23",words)]
        units[40].append(case(v+1,"3데이터와 21 NOP를 포함한 고정 24워드" if v==0 else "서로 다른 고정 24워드",
            "읽기중 플래그=0(읽지 않음), 이전 읽기 인덱스=17, PCM_mode=3(SpecificCommand), "
            "PCM_mode_next=4(NopStandby)로 둔다. " +
            ("입력[0..2]=[17,34,51], 입력[3..23]=152917(NOP)로 채운다. " if v==0 else
             "입력[i]=0x50000|(i<<9)|(0x80+i), i=0..23으로 모두 채운다. ") +
            "fn_PcmBitStream_Mode_SepcificCommand()를 1회 호출한다. 길이 인자는 없고 호출자가 NOP까지 채운다. "
            "읽기중 플래그=1은 CFX가 입력 버퍼를 읽는 중임을 뜻한다. 인덱스는 각 쓰기 이후 갱신된다.",f))
    return units


def extra_cases(units: dict[int,list[dict]]) -> None:
    names=["일반 맵/왼쪽 귀", "일반 맵/오른쪽 귀/CIS", "일반 맵/귀 미지정", "일반 맵/I2S 스트리밍", "매핑 Live/날짜 유지", "맵 요청 없음", "FS 대역 수 불일치 관찰"]
    for v in range(7):
        live,idle,mismatch=v==4,v==5,v==6
        front=1 if v==0 else 2 if v in (1,3) else 0
        f=[field("[남은 맵 요청, 재적재 확인, 계산 완료 요청, NofM phase, 패킷 헤더, 활성 마이크 수, front_mic, ADC 쓰기수, DIO 쓰기수, ch0 지연 선택, ch2 지연 선택, PCM 쓰기수]",
                 [0,int(not idle),0,1 if idle or v==1 else 0,123 if idle else 0x48000,
                  1 if live or v==3 else 2,front,0 if live or idle else 2,0 if idle else 2,
                  -1 if live or idle else int(front==2),-1 if live or idle else int(front==1),0 if idle else 24]),
           field("PCM 최종 슬롯 j=0..23",[-777 if idle else NOP]*24),
           field("날짜 6워드",[(70 if live or idle else 10)+i for i in range(6)])]
        f += [field(name+", 대역 i=0..31",[-100-a]*32 if idle else arr) for a,(name,arr) in enumerate(zip(MAP_NAMES,map_arrays()))]
        f += [field("FFT bin 목적지 i=0..255",[-1]*256 if idle or mismatch else [i%32 for i in range(256)]),field("같은 이벤트 재호출 때 PCM 쓰기수",0)]
        units[11].append(case(100+v,names[v],MAP_INPUT +
            f" 전략={1 if v==1 else 2}({'CIS' if v==1 else 'NofM'}), mapNum={-1 if live else 0}({'Live' if live else '일반 맵'}), "
            f"귀 위치={1 if v==0 else 2 if v in (1,3) else 0}(0=미지정/1=왼쪽/2=오른쪽), "
            f"I2S 원시 상태={1 if v==3 else 2}(1=활성/2=비활성), 맵 요청={0 if idle else 1}, FS 대역={31 if mismatch else 32}로 둔다. "
            "현재 내부기=1, front_mic=0, 마이크 수=2, NofM phase=1(뒷 8패킷 처리 차례), 헤더=123, "
            "로컬 대역수=1, 재적재 확인=0, 계산 완료 요청=0으로 초기화한다. "
            "각 로컬 맵 배열 a=0..6의 i=0..31은 -100-a, FS bin[i]=i%32, 목적지 bin[i]=-1(i=0..255)로 둔다. "
            "ADC 지연 기록은 -1(호출 없음), ADC/DIO/PCM 호출 횟수=0, PCM 슬롯=-777로 초기화한다. "
            "Normal_PowerMode_event_mapChange()를 호출하고 결과를 저장한 뒤 trace를 초기화하여 한 번 더 호출한다. "
            "요청=1은 처리 요청, 0은 요청 없음/소비됨이다. 재적재 확인=1은 CFX의 완료 게시다. "
            "front_mic 0=미지정, 1=왼쪽(DMIC2), 2=오른쪽(DMIC1). ADC 지연 선택 0=기본, 1=0.025표본 지연 설정 호출이다. "
            "이 숫자는 모의 SDK 호출의 기록이며 실제 ADC 지연 측정값이 아니다.",f,extra=True,
            mode="PC 전용 / 모의 SDK / RAM / 특성화 관찰" if mismatch else "PC 전용 / 모의 SDK / RAM",
            limitation="FS 대역 불일치인데도 재적재 확인=1을 게시하고 이전 bin=-1을 유지한다. 이 사전 특성화 비교값의 일치는 안전한 실패 처리가 통과했다는 뜻이 아니며 정책 판정 보류다." if mismatch else ""))
    f=[field("[채널당 프레임, 블록당 전송 채널, 직전 오디오 볼륨, 직전 자극 볼륨, 계산완료 요청]",[3,8,-1,-1,0]),
       field("대역 i=0..31 순서의 [T 코드, C 코드]",[n for i in range(32) for n in [50+i,150+i]]),
       field("지워진 HEAR vMag RAM i=0..255",[0]*256),
       field("대역 i=0..31 순서의 [Cnew, A(Q16.8), B(Q16.8)]",[n for i in range(32) for n in [150+i,12800,(50+i)*256]]),
       field("재호출 뒤 [vMag[0], 직전 자극 볼륨, 직전 오디오 볼륨]",[77,4,1])]
    units[11].append(case(107,"CM3 계산 완료 소비 후 볼륨 재계산",
        "CM3 계산 완료 요청=1(결과 준비됨), frameNumPerChannel=3, transferableFrameNum=8로 둔다. "
        "대역 i=0..31에 공유 T=50+i/C=150+i(DAC 코드), 로컬 T/C=-1, xmin=0/xmax=2를 설정한다. "
        "모의 HEAR vMag[i]=i+1(i=0..255)로 둔 뒤 Normal_PowerMode_event_audioParameterCalculation()을 호출한다. "
        "이후 (자극볼륨,오디오볼륨)=(4,1), power override=1로 볼륨 이벤트를 호출한다. "
        "마지막으로 vMag[0]=77을 넣고 계산 완료 함수를 재호출한다. 이미 요청을 소비했으므로 77을 지우지 않아야 한다. "
        "-1 직전 볼륨은 다음 볼륨 이벤트에서 다시 계산하도록 하는 미반영 표시다.",f,extra=True))
    for v in range(4):
        active=v<2
        f=[]
        for b in range(3):
            f += [field(f"블록 {b}: [PCM 쓰기 발생(1=있음), 다음 인덱스, 다음 NofM phase]",
                        [int(active),(b+1)*12%32 if v==0 else (8 if b%2==0 else 0) if v==1 else 0,1 if v==1 and b%2==0 else 0]),
                  field(f"블록 {b}, 대역 i=0..31 순서의 [로컬 진폭, 공유 출력 진폭, 대표값, >>4 스케일값]",
                        [n for i in range(32) for n in ([102-i,102-i,32,2] if active else [-55,-66,-77,-88])])]
            words=[]
            for j in range(24):
                stride=3 if v==1 else 2
                idx=ORDER[(b%2)*8+j//stride] if v==1 else (b*12+j//stride)%32
                words.append((NOP if j%stride else packet(0,idx,102-idx)) if active else -777)
            f.append(field(f"블록 {b} PCM 최종 24슬롯",words))
            if v==1:
                f.append(field(f"블록 {b}, 선택 위치 i=0..15 순서의 [오름차순 선택 대역, 교차 배치 대역]",[n for i in range(16) for n in [i,ORDER[i]]]))
        units[33].append(case(108+v,["CIS 전체 호출 경로", "NofM phase 0/1/0 호출 경로", "medium 전략의 무동작 관찰", "알 수 없는 전략 99의 무동작 관찰"][v],
            f"전략 값={[1,2,3,99][v]}(1=CIS/2=NofM/3=medium/99=미정의), 대역 수=32, 첫 위상=0, "
            "묵음 모드=2(T..C 범위 제한 방식), 표시 자극 출력 플래그=0(표시 자극 끔), power override=1로 둔다. "
            "FFT bin i=0..255에 대역표=i%32, 모의 HEAR vMag=32를 넣는다. 대역 i=0..31에 "
            "A=256/B=(100-i)*256(Q16.8), Cnew=255, T=0, 로컬 진폭=-55, 공유 출력=-66, 대표값=-77, 스케일값=-88, "
            "CIS 순서=i+1, 전극=i+1, 물리표=i, 임시 패킷=-99를 설정한다. NofM phase=0(새 계산), "
            "직전 마지막 대역=15, 전송 인덱스=0으로 둔다. " +
            ("간격=3/전송량=8이다. " if v==1 else "간격=2/전송량=12이다. ") +
            "stimulationStrategy()를 3회 호출한다. 매 호출 모의 PCM은 -777로 초기화한다. " +
            ("첫 호출 뒤 vMag 256개를 0으로 바꾸고 둘째 호출 뒤 모두 32로 복구한다. 두 번째 phase는 재계산 없이 이전 패킷을 사용해야 한다. " if v==1 else "") +
            "활성 전략의 독립 기대식은 대표값=32, 스케일값=32>>4=2, 고정 power=131072, "
            "진폭=((256*131072>>16)+(100-i)*256)>>8=102-i이다.",f,extra=True,
            mode="PC 전용 / 모의 IO / RAM / 특성화 관찰" if v>=2 else "PC 전용 / 모의 IO / RAM",
            limitation="PCM을 쓰지 않고 기존 상태가 남는 특성화 관찰이다. NOP 출력 또는 차단을 안전 정책으로 요구할지는 미정이며 제품 PASS로 판단하지 않는다." if v>=2 else ""))


def value_text(values: list[int]) -> str:
    """범위가 명확한 반복/등차열은 수식, 나머지는 모든 원소를 표시한다."""
    if len(values)==1:
        return str(values[0])
    if len(set(values))==1:
        return f"모든 i=0..{len(values)-1}: {values[0]}"
    if len(values)>4 and all(values[i]-values[i-1]==values[1]-values[0] for i in range(1,len(values))):
        return f"i=0..{len(values)-1}: {values[0]} + ({values[1]-values[0]})×i"
    if values==[i%32 for i in range(len(values))] and len(values)==256:
        return "i=0..255 전체: i % 32 (0..31을 정확히 8회 반복)"
    return ", ".join(map(str,values))


def table(headers: list[str], rows: list[list]) -> str:
    return "\n".join(["| "+" | ".join(headers)+" |", "|"+"---|"*len(headers)] +
                     ["| "+" | ".join(str(c).replace("|","\\|").replace("\n"," ") for c in row)+" |" for row in rows])+"\n"


def field_table(fields: list[dict]) -> str:
    result=[]
    offset=0
    for f in fields:
        vals=f["values"]
        end=offset+len(vals)-1
        description=value_text(vals)
        if len(vals)>16 and description==", ".join(map(str,vals)):
            for start in range(0,len(vals),16):
                stop=min(start+16,len(vals))
                result.append([f"{offset+start}..{offset+stop-1}",
                               f"{f['field']} / 필드 내 나열 위치 {start}..{stop-1}",
                               ", ".join(map(str,vals[start:stop]))])
        else:
            result.append([str(offset) if len(vals)==1 else f"{offset}..{end}",f["field"],description])
        offset=end+1
    return table(["payload 인덱스(0기준)","관찰할 필드 / 원소 순서", "사전 기대값(정수, 오차 0)"],result)


def rtt_summary(core: str, unit: int, key: str | int, count: int, status="PASS", failed=0) -> str:
    return f"[UT-{core}-{unit:03d}] C{key} {status} N={count} F={failed}"


def rtt_section(core: str, unit: int, cases: list[dict]) -> str:
    supported=[c for c in cases if not c['extra']]
    text="## 실제 reporter가 지원하는 통과 예상 요약행\n\n"
    text+="아래는 장치에서 수집한 로그가 아니라, 해당 비교 레코드가 모두 일치할 때 구현된 RTT reporter가 출력하는 요약행이다. "
    text+="C는 본문의 case/vector 번호, N은 머리말 4워드를 제외한 비교 원소 수, F는 불일치 수다. 값 전체는 앞의 표를 사용한다. "
    text+="시험 결과의 actual/expected 배열을 고정한 상태에서 CM3 전경 reporter로 전달하고 전체 배출 완료 후 결과를 해제한다. "
    text+=("`tdc_ut_report_cfx_begin`에 IO 결과 배열의 현재 빌드 XMEM 워드 주소를 전달한다. " if core=='CFX' else
           '`tdc_ut_report_cfx_core_begin("HEAR", ...)`에 완료 비교 배열의 현재 빌드 XMEM 워드 주소를 전달한다. ')
    text+="바이트 주소나 다른 빌드의 주소를 재사용하지 않는다.\n\n"
    text+="```text\n"+"\n".join(rtt_summary(core,unit,c['key'],len(payload(c))) for c in supported)+"\n```\n\n"
    if any(c['extra'] for c in cases):
        text+="PC 전용 100번대 case는 위 RTT 비교 레코드가 아니다. 해당 case에 RTT PASS 행을 만들지 않는다.\n\n"
    if core=='HEAR':
        text+="시간 초과 뒤 늦게 완료된 경우에는 정상 벡터 값이 일치하더라도 다음 실패 레코드가 추가된다. 전체 유닛 PASS로 처리하지 않는다.\n\n"
        text+="```text\n"+rtt_summary(core,unit,99,1,'FAIL',1)+"\n```\n\n"
        if unit==2:
            text+='raw FFT/vMag/블록 지수는 비교 레코드 배출 후 `tdc_ut_report_cfx_values_begin("HEAR",2,case,word,count,done)`으로 각각 배출한다. '
            text+="C80은 FFT 512워드, C81은 vMag 256워드, C82는 E 1워드다. 각 원본 벡터 1..5와 캡처의 대응을 함께 보존한다. "
            text+="이 raw 출력은 expected가 없으므로 F=0이어도 PASS가 아니라 OBSERVED이다.\n\n```text\n"
            text+="\n".join(rtt_summary(core,unit,k,n,'OBSERVED') for k,n in [(80,512),(81,256),(82,1)])+"\n```\n\n"
    return text


def records(capture: dict, value_key="expected") -> dict[str,list[int]]:
    data=capture[value_key]
    result={}
    p=0
    while p<len(data):
        marker,unit,key,size=data[p:p+4]
        assert marker==-999 and unit==capture["unit"] and str(key) not in result
        result[str(key)]=data[p+4:p+4+size]
        p+=4+size
    assert p==len(data)
    return result


def link_from(document: Path, path: str | Path, label: str | None=None) -> str:
    rel=os.path.relpath(ROOT/path, (ROOT/document).parent).replace("\\","/")
    return f"[{label or Path(path).name}]({quote(rel,safe='/.-_')})"


def cfx_common(unit: int) -> str:
    return f"""# CFX-{unit:03d} {TITLES[unit]}

구현된 시험의 입력과 사전 기대 결과를 정리한 자료다. 아래의 PC 관찰은 저장된 실행 근거이며 실제 장치 합격을 뜻하지 않는다.
이 문서의 모든 정수 비교 허용오차는 **0**이다. 배열 구간의 양 끝을 포함하고, 별도 지정이 없으면 인덱스는 0부터 시작한다.

## 시험 환경과 실행/판정 절차

1. PC 시험은 실제 제품 함수체와 기존 모의 FIFO/FS/SDK 경계를 사용한다. 소스 변경 없이 기존 fixture를 읽은 결과다. 입력/기대값은 아래 case별로 확인한다.
2. 타깃 대응 벡터는 시험 빌드에서 `tdc_ut_cfx_io_run_unit({unit})`에 해당한다. 호출 전 오디오/PCM 생산자, 정상 CFX 반복, 관련 ISR 소비자와 CM3 공유값 쓰기를 정지해야 한다. 진입점이 이를 자동 정지시키지 않는다. 본 작업에서 장치 실행/다운로드는 하지 않았다.
3. `g_tdc_ut_io_override_enabled`는 0=원래 하드웨어 경로, 1=모의 RAM 경로다. 일반 벡터 호출이 1로 바꾸어 비교한 뒤 0으로 되돌린다. 결과는 `[-999,unit,case,payload 길이,payload...]`이며 actual/expected 버퍼를 별도로 보존한다. 아래 표 인덱스는 메타데이터 4워드를 제외한 payload 기준이다. PC 전용 100번대 case는 별도 CFX_EXTRA 배열이며 이 4워드 머리말이 없다.
4. 반환 0은 실행/기록 완료다. `done=1`도 기록 완료이며 PASS가 아니다. `failed=0`은 이 벡터에서 비교한 값의 불일치가 없음, `overflow=0`은 결과 공간/모의 쓰기 범위 초과가 없음을 뜻한다. 제품 상태는 저장/복원하지만 캡처는 보존하므로 소비 완료 후 `tdc_ut_cfx_io_release()`로 done을 0으로 해제한다.
5. 최적화 O0/O2와 독립 재실행에서도 입력/기대값/허용오차는 동일하게 적용한다. 중복 벡터로 세지 않는다. 제공된 이 IO 실행 명령에는 `-O` 옵션이 없고 각 벡터를 두 번 실행했다. 이 근거만으로 O2 확인 완료를 주장하지 않는다. O0/O2 각각의 결과를 별도 실행 근거로 확인해야 한다.

보호 조건에서 HEAR `pending`은 1=처리 중/0=처리 중 아님, 각 실행기의 `done`은 1=미해제 결과 있음/0=없음이다. `target_needed`는 1=실제 HEAR 확인 필요/0=완료 관찰 기록 후이며, `vmag_count`는 보존한 크기 값 개수다. power/AGC override 역시 0=원래 경로, 1=시험 대체 경로 사용 중을 뜻한다. 오류 반환 -1은 미지원 입력, -2는 진행 중이거나 이전 결과가 남은 보호 거부, -4는 실제 타깃 필요다.

PC는 CFX의 24비트 int/48비트 long을 그대로 재현하지 않는다. 기존 checker는 제한된 입력의 long 중간값을 호스트 long long으로 넓힌다. 따라서 값/분기/기록 순서 확인과 타깃 ABI, 포화, IRQ 경쟁 및 실제 FIFO 동작을 구분한다.

## 입력 벡터와 기대 결과

"""


def cfx_notes(unit:int) -> str:
    notes={
        7:"두 번의 입력 후 최신/직전 블록과 두 마이크의 분리가 비교 대상이다. 샘플링/마이크 하드웨어와 1 ms 처리 시간은 이 복사 시험에서 측정하지 않았다.",
        10:"RF 연결 성공이나 실제 자극 출력은 확인하지 않았다. 매핑 앱 연결 중 해제/동일 내부기 유지/다른 번호 재연결은 이 구현 벡터에 없으므로 전체 상태 전이를 검증했다고 쓰지 않는다.",
        11:"볼륨 5(자극), 11(오디오)처럼 양의 상한 초과 입력은 테이블 범위 검사가 없어 실제 호출하지 않았다. 거부/보존 기대 정책도 확정되지 않았으므로 미실행/판정 보류다. 100..107은 기존 타깃 훅으로 격리할 수 없는 ADC/DIO/HEAR RAM을 PC fixture로 대체한 전용 사례다. 모의 ADC/DIO 호출 성공을 실제 하드웨어 설정 완료로 쓰지 않는다. 볼륨 변경의 24회 쓰기는 기존 함수가 생성하는 NOP 전송이다. 첫 볼륨 묶음은 횟수만 직접 비교하며 100번대는 최종 24워드까지 비교한다.",
        12:"bipolar의 실제 enum은 4이다. 1이라는 오래된 주석을 입력값으로 사용하지 않는다. 날짜 제외 복사는 목적지 날짜를 보존한다. 동시 CM3 쓰기와 원자적 일괄 복사는 이 정지 상태 시험으로 보장되지 않는다.",
        15:"known mismatch: case 1의 payload[19], 전체 flat offset 23에서 copy_done 기대 1/PC 실제 0이다. 복사 함수에 완료 플래그를 1로 만드는 대입이 없다. 기대값을 0으로 바꾸지 않는다. IRQ 없음/링버퍼 wrap/8블록 충전/underrun 복귀/유효하지 않은 포인터는 이 두 구현 벡터에 포함되지 않는다. 이 항목은 미검증이며 값을 임의 확정하지 않는다.",
        19:"현재 4단계의 시작/끝/종료 상태만 시험했다. 중간 단계 1/2/7/8, 음수/포화 입력 및 다른 단계 수 구성은 이 벡터의 직접 비교 범위가 아니다. 시험 목적은 배열의 페이드 계산이며 추가 파형 측정을 임의의 통과 조건으로 두지 않는다. 실제 스트리밍/음향 동작은 미실기다.",
        26:"검증 대상은 역순 16회 쓰기다. 실제 FIFO 포인터 이동/512표본 누적/HEAR 완료는 미실기이며 여기서 합격으로 확대하지 않는다.",
        27:"입력 bin의 -1/0..31 의미와 개수 비교만 확인했다. 복사 함수는 값의 범위 검사기를 갖지 않는다. 동일 개수이지만 bin=32 이상인 표의 거부 정책/후속 소비자 안전은 이 시험으로 확정하지 않는다. 맵 이벤트가 불일치 상태에서 완료를 게시하는 사례는 CFX-011 case 106의 특성화 관찰이다.",
        33:"case 20/22의 stride=2/transferable=12는 CM3의 floor(24/2)=12와 맞는 유효 설정이다. 1밴드 2개와 11밴드 17개, 합계 19개 불일치다. case 21의 stride=1이면 CM3 계산은 24이므로 transferable=12와 조합할 수 없다. 이 비정상 조합의 2개 불일치를 별도로 둔다. case 23(12/2/12)와 30..32는 비교 일치다. 전략 110/111의 안전한 무출력/차단 정책은 미정이다. 실제 PCM 전송/내부기 펄스는 측정하지 않았으며 이 문서에 물리 파형 통과 조건을 추가하지 않는다.",
        36:"위상0=음극 선행, 위상1=양극 선행이라는 설정 코드와 패킷의 bit15 전달을 비교한다. 실제 펄스의 극성/전극 전류 실측은 하지 않았다. type4와 하위2비트0은 고정 필드이며 가변 입력 API가 아니다. 범위를 넘는 위상/전극/진폭의 거부 시험은 구현되지 않았다.",
        37:"이 문서는 실제 패킷 구성/분할 쓰기를 다룬다. 앞단 피크 선택/교차 배치는 별도 유닛이며 여기서는 미리 만든 목록을 입력한다. 모의 최종 24슬롯 비교를 실제 FIFO 파형/연속 1 ms 완료로 해석하지 않는다.",
        40:"읽기중 플래그/이전 인덱스/완료 후 모드와 입력 보존을 비교한다. 별도 busy-reject API나 길이=3 API는 없다. CM3 동시 덮어쓰기 차단이나 실제 PCM/RF 전송 성공은 이 시험에서 입증하지 않았다.",
    }
    return notes[unit]


def render_cfx(unit:int, cases:list[dict], capture:dict, evidence:dict) -> str:
    doc=OUT/"CFX"/f"CFX-{unit:03d}.md"
    text=cfx_common(unit)
    expected_records=records(capture)
    actual_records=records(capture,"actual")
    for c in cases:
        text+=f"### case {c['key']} — {c['title']}\n\n{c['input_and_action']}\n\n"
        text+=f"실행 구분: {c['mode']}. "
        text+=("아래는 사전 특성화 비교값이다. 정상 통과 조건이 확정된 것으로 해석하지 않는다.\n\n" if "특성화" in c['mode'] else "다음 필드를 모두 비교한다.\n\n")
        text+=field_table(c["expected_fields"])+"\n"
        if c["limitation"]:
            text+=c["limitation"]+"\n\n"
        if not c["extra"]:
            mismatch=[(i,a,e) for i,(a,e) in enumerate(zip(actual_records[c["key"]],expected_records[c["key"]])) if a!=e]
            if mismatch:
                text+="저장된 PC 관찰의 불일치(사전 기대값은 유지):\n\n"+table(["payload 인덱스","PC actual","사전 expected"],mismatch)+"\n"
    text+=rtt_section('CFX',unit,cases)
    known_lines=[]
    for c in cases:
        if not c['extra']:
            failed=sum(a!=e for a,e in zip(actual_records[c['key']],expected_records[c['key']]))
            if failed:
                known_lines.append(rtt_summary('CFX',unit,c['key'],len(payload(c)),'FAIL',failed))
    if known_lines:
        text+="현재 PC에서 관찰한 불일치가 타깃에서도 동일하게 재현될 때의 요약은 다음과 같다. 실제 RTT 수집 기록은 아니다. 사전 통과 기대값은 변경하지 않는다.\n\n```text\n"+"\n".join(known_lines)+"\n```\n\n"
    text+="## IO 검사기의 오류 입력, 결과 대기와 복원\n\n"
    text+=table(["조건/입력과 실행","사전 기대 결과"],[
        [f"본 유닛 run_unit({unit})을 각각 두 번 독립 실행하고 실행마다 release", "run=0, done=1, overflow=0, 본문의 알려진 불일치 수 유지. 제품 상태 복원"],
        ["본 유닛 결과를 해제하지 않은 done=1 상태에서 run_unit(7), hear_start(1,1,1)","둘 다 -2. actual 전체768워드 불변. 이후 cfx_io_release=0"],
        ["pure vector done=1 / HEAR pending=1 / HEAR done=1을 각각 따로 설정하고 run_unit(7), hear_start(1,1,1)","각 -2. actual/expected 전체768워드와 공유 구조체 보존"],
        ["HEAR pending=1일 때 cfx_io_release, hear_release", "각 -2, 진행중 상태 유지"],
        ["미지원 run_unit(14), run_unit(99)","각 -1; 지원하지 않는 ID를 실행했다고 표시하지 않음"],
        ["일반 PC에서 hear_start(1,1,0), (1,1,1), (2,1,1) 및 hear_poll", "모두 -4. HEAR done=0, target_needed=1, vmag_count=0"],
    ])+"\n"
    text+="다음 초기화/복원 확인도 각 호출에 적용한다. 기존 PC fixture는 PCM 진폭[i]=101+i, 패킷[i]=201+i, 물리표[i]=31-i(i=0..31), "
    text+="선택순서[i]=i와 AGC[i]=301+i(i=0..15), bin[i]=i%32(i=0..255), I2S 버퍼[b][i]=100*b+i(b=0..7,i=0..15), "
    text+="DMIC[m][b][i]=1000*m+100*b+i(m=0..1,b=0..1,i=0..15), UART[i]=8000+i(i=0..511)를 준비한다. "
    text+="I2S 원시 상태 RESET=0, in=4/out=3/copy_count=5/state=2/sub_state=8, 공유 볼륨=2/2, 내부기 번호=2, 특정명령 읽기=0/index=17도 복원 비교한다. "
    text+="모의 캡처는 보존되고 제품 상태는 호출 전 값으로 되돌아와야 한다. 공유 구조체 전체 비교와 위 배열 비교가 기존 checker에 있다. "
    text+="flat 형식 검사도 각 실행에 있다. marker=-999, actual/expected 머리말 4워드 일치, length>0, 마지막 레코드가 count와 정확히 맞고 버퍼를 넘지 않아야 한다.\n\n"
    if unit in (11,33):
        text+="### PC 전용 추가 사례의 서로 다른 초기 상태와 보호 입력\n\n"
        text+="추가 복원 시험은 위 기본값과 다른 초기 상태로 일반 유닛 11/33을 각각 두 번 실행한다. 대역 i=0..31에 "
        text+="로컬 맵 T(µA)=301+i, 계산 T 코드=401+i, A=501+i, B=601+i, 계산 C 코드=701+i, Cnew=801+i를 둔다. "
        text+="그 뒤 100..111을 각각 두 번 실행하며 공유 구조체/UART 전체, 아래 모든 배열과 스칼라를 백업/복원 비교한다. 초기화되지 않은 상태는 0으로 가정하지 않고 호출 직전 값 자체를 기준으로 보존한다.\n\n"
        text+=table(["복원 영역","빠짐없이 비교하는 범위"],[
            ["날짜, 로컬 맵 배열", "날짜[0..5], 자극/기준 전극/CIS 순서/T/C/xmin/xmax 각각[0..31]"],
            ["계산/패킷 배열","계산 T/C/Cnew/A/B/진폭/물리전극표/임시 패킷/자극 데이터 각각[0..31]"],
            ["NofM/주파수/오디오", "두 선택 목록 각각[0..15], 대표값/스케일값 각각[0..31], bin/vMag RAM 각각[0..255], AGC/I2S 출력 각각[0..15], I2S[0..7][0..15], DMIC[0..1][0..1][0..15]"],
            ["스칼라", "전략/첫위상/모드/폭/대역수/표시채널/표시전류, 이전 두 볼륨/내부오디오 인덱스/현재/이전 내부기/이전 AGC 게인/연결카운터, 헤더/프레임/전송량/인덱스/NofM last/phase, front mic/마이크수, I2S in/out/count/state/substate/raw state/interrupt/copy_done, IO/power/AGC override"],
        ])+"\n"
        text+=table(["별도 경계 시나리오","예상 결과"],[
            ["IO override=1, power override=1, AGC override=1을 각각 단독 설정해 run_unit(11), run_unit(33)","각 -2. 제품 상태와 actual/expected 전체768워드 그대로"],
            ["PCM 모의 포인터=`&buffer[48]` (`top[47]`보다 하나 위), value=123, 쓰기수=0", "overflow=1, 쓰기수=0, 해당 메모리의 이전 값 보존"],
            ["정상 PCM 포인터=&buffer[47], trace 쓰기수=128, value=123", "overflow=1, 쓰기수128 유지, 대상 메모리 이전 값 보존"],
            ["결과 count=768로 채운 상태에서 put(123,123)", "overflow=1, count768 유지, 범위 밖 결과 쓰기 안 함"],
        ])+"\n이 검사는 IO fixture의 보호 검사다. 제품의 잘못된 배열 접근을 직접 실행해 거부 동작을 확인한 것이 아니다. 공통 로그/report/감사기록 및 실행기 자체의 시험은 공통 문서에서 설명한다.\n\n"
    text+="## 저장된 PC 관찰과 남은 한계\n\n"
    text+=f"제공된 checks.json의 CFX-{unit:03d}는 두 번 실행했고, 1회당 메타데이터 포함 {capture['word_count']}워드와 불일치 {capture['host_mismatch_count']}개를 기록했다. "
    text+="새 장치 시험 결과가 아니며 본 문서 작성 중 제품 시험을 재실행하지 않았다. "
    if unit in (11,33):
        keys=[int(c['key']) for c in cases if c['extra']]
        text+=f"PC 전용 추가 case {min(keys)}..{max(keys)}도 각각 두 번 기록되어 있다. 106/110/111의 일치는 특성화 비교 일치일 뿐 안전한 실패 정책의 PASS가 아니다. "
    text+="\n\n"+cfx_notes(unit)+"\n\n"
    lines=[line for line in evidence['stdout'].splitlines() if line.startswith(f"CFX-{unit:02d} ")]
    text+="구현된 PC 표준출력 예시(기존 저장 로그 그대로):\n\n```text\n"+"\n".join(lines)+"\n```\n\n"
    text+="이 출력 형식은 PC checker의 요약이다. 제품 RTT 전용 로그로 바꾸거나 구현되지 않은 PASS 문장을 추가하지 않는다. 물리 관찰은 본 벡터의 판정 항목에 포함되지 않으며 실제 장치는 미실행이다.\n\n"
    if unit in (11,12,19):
        text+="### QCC 연계 자료 대조 위치\n\n"
        text+={11:"맵/볼륨 변경과 I2S 상태에 따른 E8300의 입력 경로 선택은 위 모의 조건으로 구분했다. QCC의 실제 스트리밍 시작/중지 상태, I2S 표본 형식과 게인 자료 수령 후 같은 경계 입력을 대조한다.",
               12:"공유 맵 복사의 기대값은 E8300 내부 복사 계약이다. QCC 자료 수령 후 I2S 입력/게인과 맵 적용 경로가 연계 조건에 영향을 주는지 대조하며, 복사 성공을 QCC 내부 동작의 합격으로 확장하지 않는다.",
               19:"페이드 기대값은 E8300 배열 계산이다. QCC의 I2S 표본 형식, 시작/중단 조건과 게인 자료를 받아 본문 입력의 대응을 대조한다. QCC 출력이나 실제 음향 관찰값은 아직 없으며 임의 파형 기준을 추가하지 않는다."}[unit]+" QCC 내부 자료 및 실제 연계 결과는 대기 중이다.\n\n"
    text+="## 근거 소스와 관찰 자료\n\n"
    sources=[RUNNER,"src/1__cfx/test/tdc_ut_io_cfx.h","tests/unit/tdc_ut_cfx_io_check.py","tests/unit/tdc_ut_cfx_io_coverage.json","tests/unit/tdc_ut_cfx_io_criteria.py","tests/unit/tdc_ut_cfx_io_criteria.json",EVIDENCE,REPORT,LOG,"tests/unit/README.md"]
    if unit in (11,33): sources.append(EXTRA)
    sources += ["src/1__cfx/"+p for p in PRODUCTS[unit]]
    if unit==33: sources.append("src/2__cm3/Cortex-M3-src/signalProcessing/stimulationParaCal.c")
    if unit==36: sources.append("src/2__cm3/Gen1_5/FS/ci_map.c")
    text+="\n".join("- "+link_from(doc,p) for p in sources)+"\n"
    return text


def hear_lifecycle() -> str:
    return """## 초기화, 실제 완료 관찰과 판정

1. 실제 타깃에서는 HEAR가 초기화되어 유휴 상태인지 확인하고 오디오/PCM 생산자, CFX 정상 루프, 관련 공유값 쓰기를 정지한다. HEAR 완료 ISR은 완료를 전달할 수 있어야 한다. `hardware_ready=1`은 호출자가 이 준비를 확인했다는 값, 0 또는 1 이외 값은 준비 미확인이다. 시험 함수가 HEAR를 초기화/리셋하지 않는다.
2. `tdc_ut_hear_start(unit,vector,1)`로 아래 입력을 적용하고 해당 체인을 요청한다. `pending=1`은 진행 중/메모리 사용 중, `done=0`은 아직 결과 기록 전이다. `function_chain0/1`은 0=해당 완료 미수신, 1=해당 완료 수신이다. 시작 전 둘 다 0이어야 하며, 기존 완료 표시가 1이면 지우지 않고 -2(BUSY)로 거부한다.
3. `tdc_ut_hear_poll()`은 한 번 확인하고 완료 전 1을 반환한다. 실제 완료를 받으면 비교와 raw 캡처, 입력/출력 메모리 및 이전 완료 표시 복원을 수행한다. 반환 0과 done=1은 기록 완료일 뿐 PASS가 아니다. `failed=0`, `overflow=0`, `timed_out=0` 및 아래 비교값이 일치해야 해당 벡터가 통과한다.
4. 결과 소비 전에 재시작하지 않는다. `tdc_ut_hear_release()`는 pending=1일 때 -2이며, pending=0에서 done을 0으로 해제한다. CFX IO와 HEAR는 같은 결과 배열을 사용하므로 동시 실행하지 않는다. H01/H23 scratch까지 원상 복원했다는 보장은 없고 정상 처리 재시작이 필요하다.
5. 100000번째 완료 없는 poll부터 -5(TIMEOUT)다. 이는 호출 횟수이며 ms/초가 아니다. pending=1/done=0/timed_out=1을 유지하고 메모리를 복원하지 않는다. 뒤늦은 완료도 case 99 `[actual=1, expected=0]`을 추가하여 시간 초과 실패를 남긴다. 그때 poll 반환 0이어도 failed가 0이 아니다.

일반 PC IO checker에서 start/poll은 -4(TARGET_REQUIRED)를 반환하며 가속기 완료를 만들지 않는다. unit가 1/2 이외, HEAR-001 vector가 1 이외, HEAR-002 vector가 1..5 이외이면 -1(UNSUPPORTED)이다. 미해제 CFX/HEAR 결과, CFX 순수벡터 done, HEAR pending, IO override 사용 중이면 시작은 -2다. 이 가드는 원자적 잠금이 아니므로 호출 전 정지 조건은 별도로 필요하다.

위 미지원 HEAR unit/vector 범위와 실제 타깃의 pending 없는 poll(-1)은 코드 분기 분석 결과다. 제공된 PC 합성 검사기가 그 모든 조합을 실행했다는 뜻은 아니다. 실제로 입력한 오류 주입/거부 사례는 아래 PC 검사 절에 구별해 적었다.

O0/O2 및 반복 실행에도 같은 입력/기대값/허용오차를 적용한다. 저장된 합성 RAM 실행 명령에는 최적화 옵션이 없으며 O2/실제 HEAR 검증 완료로 확대하지 않는다. 최적화별 확인은 같은 벡터의 실행 조건으로 기록하고 문서를 중복 생성하지 않는다.

"""


def hear_cases(reference:dict) -> dict[int,list[dict]]:
    timing=reference['hear1_timing']
    units={1:[case(1,"Event0 후 최종 절대값/최댓값",
        "HEAR_ADDR_AUDIO_MIX[0..15]에 [0,-32,17,-100,99,0,0,0,0,0,0,0,0,0,0,0]을 넣는다. "
        "값은 오디오 정수 코드이다. tdc_ut_hear_start(1,1,1)는 CMD0을 요청하고 실제 Event0 후 poll로 비교한다. "
        "vAbsolute 직후 중간값은 [0,32,17,100,99,0,0,0,0,0,0,0,0,0,0,0]이지만 MaxElement가 같은 주소의 첫 워드를 최대값 100으로 덮어쓴다. "
        "중간값은 microcode 순서를 읽는 분석(A) 대상이고 완료 시 실행 비교값과 다르다.",
        [field("완료 후 ABS[i], i=0..15",timing['after_maxelement_event0_execution_expected']),field("[function_chain0, function_chain1]",[1,0])],
        mode="실장치 전용 / 미실행 / 중간값은 분석")],2:[]}
    labels=["영 입력", "bin 32 정현파", "bin 64 정현파", "DC 입력", "Nyquist 교대 입력"]
    for v in reference['vectors']:
        no=v['vector']
        expected=[field("FFT 전체 512워드 OR / vMag 전체 256워드 OR",[0,0]),field("[chain0,chain1]",[0,1])] if no==1 else [
            field("실제 블록 지수 E가 0..13 범위인지(1=범위 안)",1),
            field("선택 9 bin × 2 성분의 독립 참조 허용오차 비교(1=범위 안)",[1]*18),
            field("vMag i=0..255 전체 허용오차 비교(1=범위 안)",[1]*256),field("최대 vMag의 인덱스",v['peak_bin']),field("[chain0,chain1]",[0,1])]
        units[2].append(case(no,labels[no-1],f"아래에 정의한 vector {no}의 x[n] 전체 512표본과 정확한 512워드 창을 사용한다. "
            f"tdc_ut_hear_start(2,{no},1)로 CMD1을 요청하고 Event1 완료 뒤 비교한다.",expected,mode="실장치 전용 / 미실행 / 독립 DFT 기대값"))
    return units


def hear_data_table(reference:dict, window:list[int]) -> str:
    text="## 정확한 512워드 입력 창\n\n"
    text+="창은 입력 데이터다. 아래 각 행의 순서는 인덱스 증가 순이며 0..511 전체를 표시했다. Q1.23이므로 실제 곱셈 비율은 정수/8388608이다. "
    text+="이상식은 round((0.5-0.5*cos(2π*n/511))*2^23)이고 저장 계수와의 차이는 최대 2 LSB다. "
    text+="DFT 기대값은 아래 저장 계수로 계산한다. start는 창을 교체하지 않으며 한 워드라도 다른 창이면 -6(WINDOW_NOT_READY)으로 요청 전에 거부한다.\n\n"
    text+=table(["n 시작..끝", "w[n] (왼쪽부터 순서대로 16개)"],[[f"{i}..{i+15}",", ".join(map(str,window[i:i+16]))] for i in range(0,512,16)])
    text+="\n## FFT 9개 bin의 모든 사전 기대 성분\n\n"
    text+="표의 값은 블록 지수 보정 전의 수학적 정수 영역 기준값이다. k=0의 두 번째 값은 Im(X[0])이 아니라 Re(X[256])이다. "
    text+="k>0의 두 값은 Re(X[k]), Im(X[k])이다. raw 배열에서는 [2*k], [2*k+1]에 대응한다.\n\n"
    rows=[]
    for v in reference['vectors']:
        for j,k in enumerate(v['fft_reference_bins']):
            r,im=v['fft_unscaled_packed_re_im'][2*j:2*j+2]
            rows.append([v['vector'],k,2*k,2*k+1,r,im,64+(abs(r)+999)//1000,64+(abs(im)+999)//1000])
    text+=table(["vector","k","첫 raw 인덱스","둘째 raw 인덱스","첫 기준값","둘째 기준값","첫 허용오차","둘째 허용오차"],rows)
    text+="\n## vMag 256개 전체 사전 기대값\n\n"
    text+="각 셀은 V[k]의 정수 영역 기준값이다. 벡터 1은 정확한 0이며 다른 벡터는 각 셀 값 R에 64+ceil(R/100)의 절대 허용오차를 적용한다. "
    text+="인덱스 0은 sqrt(Re(X[0])²+Re(X[256])²)로 계산한 packed DC/Nyquist 크기이며, 나머지 1..255는 복소수 크기다.\n\n"
    text+=table(["k","vector 1","vector 2","vector 3","vector 4","vector 5"],
                [[k]+[v['vmag_unscaled'][k] for v in reference['vectors']] for k in range(256)])
    return text+"\n"


def render_hear(unit:int,cases:list[dict],reference:dict,window:list[int]) -> str:
    title="오디오 절대값 최대값 연산" if unit==1 else "창 적용 FFT와 크기 연산"
    doc=OUT/"HEAR"/f"HEAR-{unit:03d}.md"
    text=f"# HEAR-{unit:03d} {title}\n\n실제 HEAR 가속기는 미실행이다. 이 문서는 사전 기대값, 분석과 PC 합성 RAM 판정기 검사를 구분한다. "
    text+="합성 이벤트/메모리는 판정기에 넣은 시험 입력이며 실제 하드웨어 결과가 아니다.\n\n"+hear_lifecycle()
    if unit==2:
        text+="## 5개 입력 벡터와 수치 비교식\n\n"
        text+="표본 인덱스 n=0..511 전체에 다음 정의를 적용한다. S의 16개 값은 "
        text+=str([0,25080,46341,60547,65536,60547,46341,25080,0,-25080,-46341,-60547,-65536,-60547,-46341,-25080])+"이다.\n\n"
        text+=table(["vector","x[n] 전체 512개 정의","기대 최대 bin"],[[1,"모든 n에서 0",0],[2,"S[n%16]을 32회 반복",32],[3,"S[(2*n)%16]을 적용(8표본 주기 64회)",64],[4,"모든 n에서 32768",0],[5,"짝수 n=32768, 홀수 n=-32768",0]])+"\n"
        text+="입력은 Q1.23 정수 코드이며 µA나 음압이 아니다. DFT는 X[k]=Σ(n=0..511) x[n]*w[n]/2^23*exp(-j*2π*k*n/512)로 독립 계산한다. "
        text+="실장치의 BFP 블록 지수 E를 읽어 FFT raw×2^E와 기준 성분 R을 비교한다. vMag는 Q2.22이므로 raw×2^(E+1)과 기준 크기 R을 비교한다. "
        text+="정규화 후 절대 차이는 FFT가 64+ceil(abs(R)/1000), vMag가 64+ceil(R/100) 이하여야 한다. "
        text+="E는 0..13, raw는 -8388608..8388607, vMag raw는 추가로 0 이상이어야 한다. 허용오차는 사전 공학 비교 기준이며 제조사 보장 정확도가 아니다. 실제 타깃 확인 전 보고서 PASS로 확정하지 않는다.\n\n"
        text+="영 입력은 위 오차식을 쓰지 않고 FFT 512개와 vMag 256개 전부 정확히 0인지 비교한다. 비영 입력은 FFT의 9개 bin만 직접 비교하고 vMag는 256개 전체를 비교한다. "
        text+="나머지 FFT raw 성분도 보존하지만 자동 통과 비교 범위는 아니다. 실제 E는 고정 기대값이 없으며 OBSERVED(관찰값)로 남긴다. "
        text+="합성 RAM의 E=4를 장치의 기대 E로 복사하지 않는다.\n\n"
    text+="## 벡터별 입력/완료 결과\n\n"
    for c in cases:
        text+=f"### vector {c['key']} — {c['title']}\n\n{c['input_and_action']}\n\n"
        text+="완료 비교 레코드는 [-999, HEAR 유닛 번호, vector, payload 길이, payload...]이다. 아래 표는 payload만 표시한다.\n\n"+field_table(c['expected_fields'])+"\n"
    if unit==2:
        text+="실제 완료 후 g_tdc_ut_hear_fft_count=512, vmag_count=256이고 두 raw 배열과 실제 E를 별도로 보존한다. "
        text+="vector 1은 메타 포함 8워드, vector 2..5는 각각 282워드다. 비영 입력의 payload[0]은 E 범위 비교, [1..18]은 9bin 성분 비교, "
        text+="[19..274]는 vMag 256개 비교, [275]는 peak bin, [276..277]은 이벤트 표시다. 참조값 자체가 모두 1이라는 뜻이 아니라 아래 수치 기준에 대한 비교 결과가 1이어야 한다.\n\n"
        text+=hear_data_table(reference,window)
    else:
        text+="ABS 최종 16워드와 이벤트 2개의 오차는 0이다. 레코드는 메타 포함 22워드이다. 입력/ABS 메모리는 실제 완료 후 백업으로 복원되므로 복원 이후 메모리를 위 최종 기대 배열과 비교하지 말고 보존 캡처를 사용한다. "
        text+="24비트 최솟값의 절대값 포화나 다른 길이 입력 시험은 현재 vector에 없으므로 검증 완료로 확대하지 않는다.\n\n"
    text+=rtt_section('HEAR',unit,cases)
    text+="## PC 합성 RAM 판정기/실행 상태 시험\n\n"
    text+="매 합성 사례 전에 fft_input[i]=1000+i/fft_output[i]=2000+i(i=0..511), vmag[i]=3000+i(i=0..255), "
    text+="mix[i]=4000+i/absolute[i]=5000+i(i=0..15), 정확한 창, 두 이벤트=0, 요청 체인=-1(없음)로 초기화한다. "
    text+="완료 뒤 이 버퍼와 플래그가 정확히 복원되는지 확인한다. 아래 사건은 실제 HEAR 완료가 아니라 직접 주입한 입력이다.\n\n"
    if unit==1:
        text+=table(["입력/절차","예상 판정"],[
            ["start(1,1,1) 뒤 absolute[0..4]=[100,32,17,100,99], [5..15]=0을 주입하고 chain0=1로 둠","요청체인0, poll=0, failed=0, count=22, 버퍼 복원"],
            ["같은 준비에서 absolute[0]만 0인 중간 배열을 주입","poll=0이지만 failed=1(최댓값 덮어쓰기 누락을 검출), 버퍼 복원"],
            ["다른 HEAR가 pending인 상태에서 start(1,1,1)","-2, 기존 진행중 메모리 보존"]])+"\n"
    else:
        text+="정상 합성 데이터는 각 벡터 기준 R에서 E=4를 주고 FFT raw=부호 대칭 반올림(R/16), vMag raw=반올림(R/32)로 만든다. "
        text+="FFT의 비교하지 않는 나머지 워드는 0으로 채운다. 이는 실제 FFT 전체를 모사한 결과가 아니다. chain1=1을 주입한 뒤 poll=0, failed=0, counts=512/256, E=4와 복원을 확인한다. "
        text+="부호 대칭 반올림은 양수 floor((R+scale/2)/scale), 음수는 양수 절댓값 반올림의 음수이다.\n\n"
        text+=table(["경계/실패 입력","예상 판정"],[
            ["FFT: reference=1000,E=0,raw=1064 / 1065 / 1066","허용오차65: 참 / 참 / 거짓"],
            ["vMag: reference=1000,E=0,raw=537 / 538","정규화1074 / 1076, 허용오차74: 참 / 거짓"],
            ["reference=0,E=0에서 vMag raw=-1 또는 FFT raw=8388608","거짓(표현 범위 위반)"],
            ["FFT raw=0,reference=0에서 E=-1 또는 14","거짓(지수 범위 위반)"],
            ["창[123]=1로 바꾸고 start(2,2,1)","-6, pending=0, 체인 요청 없음, 작업 버퍼 유지"],
            ["시작 전에 chain1=1","-2, 기존 이벤트1 유지, 요청 없음"],
            ["vector2/E4 정상 합성 후 fft_output[65]를 정수 /2","poll=0이지만 failed>0; 2배 스케일 오류 검출"],
            ["같은 준비에서 vmag[32]를 ×2","poll=0이지만 failed>0; 크기 스케일 오류 검출"],
            ["같은 준비에서 vmag[17]=10000","poll=0이지만 failed>0; 비피크 bin 오염 검출"],
            ["같은 준비에서 vmag[17]=-1","poll=0이지만 failed>0; 음수 크기 거부"],
            ["같은 준비에서 BLOCK_EXP=14","poll=0이지만 failed>0; 지수 범위 거부"],
            ["vector1/E4 합성 후 fft_output[223]=1","failed=1; 영 입력은 선택9bin 밖도 정확히0 검사"],
            ["vector2 시작 후 완료 없이 poll 1..99999회","각1 반환, pending=1; 미완료 메모리 유지"],
            ["100000번째 미완료 poll / 진행중 release / 새 start","각 -5 / -2 / -2, timed_out=1, input[0..1]=[0,25080] 유지"],
            ["timeout 뒤 vector2/E4 정상 합성 완료 주입","poll=0, failed=1(case99), timed_out=1, 완료 후만 복원"]])+"\n"
    text+="## 저장된 관찰, 로그와 한계\n\n"
    text+="기존 HEAR 참조 검사 JSON은 build_exit_code=0/exit_code=0, hardware_executed=false, device_pass_claimed=false를 기록한다. "
    text+="실제 장치 raw 배열/E/완료 시간은 아직 없다. 정상 PC IO에서는 target_needed=1/done=0/vmag_count=0이며 -4로 거부한다. "
    text+="예상값이 없는 raw OBSERVED 기록만 보고 PASS를 만들지 않는다. 이 유닛은 하드웨어 처리 결과를 RAM에서 읽어 수치 비교하도록 구현되어 있고 별도의 파형 측정을 통과 조건으로 임의 추가하지 않는다.\n\n"
    text+="구현된 PC 출력 예시:\n\n```text\nHEAR-1/2 TARGET_REQUIRED; host did not fabricate completion\nHEAR_COMPARATOR_LIFECYCLE_OK: synthetic comparison inputs only; accelerator NOT executed\n```\n\n"
    text+="이 두 줄은 PC 실행기/합성 RAM 검사기의 출력이다. 미구현 RTT 수치 로그가 아니며 실제 가속기 시험의 PASS 로그로 쓰지 않는다.\n\n## 근거 소스와 관찰 자료\n\n"
    sources=["src/3__hear/microcode.hct",RUNNER,"src/1__cfx/test/tdc_ut_io_cfx.h","src/1__cfx/test/tdc_ut_hear_reference.h",
             "tests/unit/tdc_ut_hear_reference.py",REF,"tests/unit/tdc_ut_hear_reference_check.py","tmp/unit-tests/hear-reference/checks.json",
             "tests/unit/tdc_ut_cfx_io_coverage.json","tests/unit/tdc_ut_cfx_io_criteria.json",EVIDENCE,REPORT,LOG,"tests/unit/README.md"]
    if unit==2: sources.append("src/2__cm3/Gen1_5/FS/ci_fft.c")
    text+="\n".join("- "+link_from(doc,p) for p in sources)+"\n"
    return text


def validate_expectations(units:dict[int,list[dict]],evidence:dict,reference:dict,window:list[int]) -> list[dict]:
    checks=[]
    captures={u['unit']:u for u in evidence['units']}
    assert set(captures)==set(units)==set(TITLES)
    assert evidence['exit_code']==0
    total=0
    extra_total=0
    for unit,cases in units.items():
        expected_records=records(captures[unit])
        regular=[c for c in cases if not c['extra']]
        assert [int(c['key']) for c in regular]==[int(k) for k in expected_records], f"case 순서/누락 {unit}"
        for c in regular:
            assert payload(c)==expected_records[c['key']], f"독립 기대값 불일치 CFX-{unit} case {c['key']}"
            total+=len(payload(c))+4
        for c in [c for c in cases if c['extra']]:
            saved=[s for s in evidence['host_only_cases'] if s['unit']==unit and str(s['case'])==c['key']]
            assert len(saved)==2 and {s['repeat'] for s in saved}=={0,1}
            for s in saved:
                assert payload(c)==s['expected'], f"추가 기대값 CFX-{unit} case {c['key']}"
                extra_total+=len(payload(c))
    checks.append({"key":"cfx_expected_source_comparison","status":"PASS","regular_flat_words":total,
                   "host_extra_words_two_executions":extra_total,
                   "meaning":"문서 독립 구성 기대값과 저장 expected 비교. 제품 통과/actual 기반 생성 아님"})
    c33=records(captures[33],"actual")
    e33=records(captures[33])
    counts={k:sum(a!=e for a,e in zip(c33[k],e33[k])) for k in ['20','21','22','23']}
    assert counts=={'20':2,'21':2,'22':17,'23':0}
    assert captures[15]['mismatches']==[{'offset':23,'actual':0,'expected':1}]
    checks.append({"key":"known_failures_separated","status":"PASS","CFX015":1,"CFX033_valid_smallband":19,
                   "CFX033_unreachable_combination":2,"cases":counts})
    sys.path.insert(0,str(ROOT/'tests/unit'))
    from tdc_ut_hear_reference import source_window, samples, dft
    assert window==source_window() and len(window)==512
    for item in reference['vectors']:
        n=item['vector']
        assert item['samples']==samples(n) and len(item['samples'])==512
        spectrum=dft(samples(n),window)
        mag=[round(math.hypot(spectrum[0].real,spectrum[256].real))]+[round(abs(v)) for v in spectrum[1:256]]
        components=[]
        for k in item['fft_reference_bins']:
            components += [round(spectrum[k].real),round(spectrum[256].real if k==0 else spectrum[k].imag)]
        assert mag==item['vmag_unscaled'] and components==item['fft_unscaled_packed_re_im']
    checks.append({"key":"hear_independent_DFT_recalculation","status":"PASS","window_words":512,"vectors":5,
                   "fft_components":90,"vmag_values":1280,"device_execution":False})
    coverage=read_json('tests/unit/tdc_ut_cfx_io_coverage.json')
    for u in coverage['units']:
        if u['core']=='CFX' and u['unit'] in TITLES:
            assert u['name']==TITLES[u['unit']]
    checks.append({"key":"unit_titles_and_owned_IDs","status":"PASS","cfx_units":12,"hear_units":2})
    local=read_json('tmp/unit-tests/cfx-io/checks.json')
    assert local['units']==evidence['units'] and local['host_only_cases']==evidence['host_only_cases']
    checks.append({"key":"saved_QA_and_tmp_observation_consistency","status":"PASS",
                   "meaning":"QA와 tmp의 두 저장 관찰 대조. 전역 baseline 검증은 주 작업 소유"})
    tree=ast.parse(Path(__file__).read_text(encoding='utf-8'))
    expected_builders={'packet','block','map_arrays','cfx_cases','extra_cases','hear_cases'}
    audited=[]
    for node in tree.body:
        if isinstance(node,ast.FunctionDef) and node.name in expected_builders:
            literals=[s.value for s in ast.walk(node) if isinstance(s,ast.Constant) and isinstance(s.value,str)]
            assert not any(s in ('actual','actual_records','host_only_cases') for s in literals)
            assert not any(isinstance(n,ast.Name) and n.id in ('evidence','captures','actual_records') for n in ast.walk(node))
            audited.append(node.name)
    assert set(audited)==expected_builders
    checks.append({'key':'expected_not_copied_from_actual','status':'PASS','audited_builders':audited,
                   'method':'기대값 구성 함수의 AST에서 관찰/evidence/actual 접근 금지 확인 + 모든 수치의 저장 expected 대조 + HEAR 독립 DFT 재계산'})
    report_source=(ROOT/REPORT).read_text(encoding='utf-8')
    log_source=(ROOT/LOG).read_text(encoding='utf-8')
    assert '"[UT-%s-%03u] C%u %s N=%u F=%u\\r\\n"' in log_source
    assert 's->expected == 0 ? "OBSERVED" : s->failed ? "FAIL" : "PASS"' in log_source
    assert 's_tdc_ut_offset = s_tdc_ut_cursor + 4U;' in report_source
    assert 's_tdc_ut_core, unit, vector,\n                              length' in report_source
    assert 'tdc_ut_report_cfx_values_begin' in report_source
    checks.append({'key':'RTT_summary_format_and_payload_count','status':'PASS','sources':[REPORT,LOG],
                   'CFX_regular_summaries':sum(not c['extra'] for cases in units.values() for c in cases),
                   'HEAR_comparison_summaries':6,'HEAR_raw_OBSERVED_summaries':3,
                   'meaning':'구현 format 문자열/expected NULL 분기와 payload 길이 사용 확인. 장치 로그 수집 아님'})
    return checks


def build() -> tuple[list[dict],dict]:
    evidence=read_json(EVIDENCE)
    criteria=read_json('tests/unit/tdc_ut_cfx_io_criteria.json')
    reference=read_json(REF)
    window=criteria['HEAR-002']['exact_window_512']
    units=cfx_cases()
    extra_cases(units)
    hear=hear_cases(reference)
    checks=validate_expectations(units,evidence,reference,window)
    captures={u['unit']:u for u in evidence['units']}
    files=[]
    manifest={"role":"cfx-io-hear","units":[],"checks":checks,
              "baseline_owner":"main","device_execution":False,"product_or_test_sources_modified":False,
              "qcc_addition_locations":[{"id":f"CFX-{u:03d}","file":(OUT/'CFX'/f'CFX-{u:03d}.md').as_posix(),
                 "heading":"QCC 연계 자료 대조 위치","status":"자료 대기 / 기존 QCC-IO-01 등록 ID"} for u in (11,12,19)],
              "optimization_note":"동일 벡터/기대값으로 O0/O2 비교. 현재 IO/합성RAM 로그는 -O 지정 없음; O2 완료 주장 안 함"}
    def case_index(c:dict,core:str,unit:int) -> dict:
        return {k:c[k] for k in ['key','title','source','mode']} | {
            'expected_fields':[{'field':f['field'],'count':len(f['values']),'expected':value_text(f['values'])}
                               for f in c['expected_fields']],
            'rtt_pass_summary':None if c['extra'] else rtt_summary(core,unit,c['key'],len(payload(c))),
            'rtt_note':'PC 전용, RTT 행 없음' if c['extra'] else '사전 통과 기대행. 실제 수집 결과 아님'}
    for unit,cases in units.items():
        ident=f"CFX-{unit:03d}"
        path=OUT/'CFX'/f'{ident}.md'
        files.append({"path":path.as_posix(),"content":render_cfx(unit,cases,captures[unit],evidence)})
        manifest['units'].append({"id":ident,"title":TITLES[unit],"file":path.as_posix(),
            "cases":[case_index(c,'CFX',unit) for c in cases],
            "limitations":[cfx_notes(unit)],"observed_mismatches_per_regular_execution":captures[unit]['host_mismatch_count']})
    for unit,cases in hear.items():
        ident=f"HEAR-{unit:03d}"
        path=OUT/'HEAR'/f'{ident}.md'
        files.append({"path":path.as_posix(),"content":render_hear(unit,cases,reference,window)})
        entry={"id":ident,"title":"오디오 절대값 최대값 연산" if unit==1 else "창 적용 FFT와 크기 연산","file":path.as_posix(),
               "cases":[case_index(c,'HEAR',unit) for c in cases],
               "limitations":["실제 HEAR 가속기 미실행. 합성 RAM 입력/이벤트는 장치 출력 아님. OBSERVED 자체는 PASS 아님."]}
        if unit==2:
            entry['numerical_expected_data']={"source":REF,"input_window_source":"src/2__cm3/Gen1_5/FS/ci_fft.c",
                "window_words":512,"input_samples_per_vector":512,"vectors":5,
                "fft_bins_per_vector":[0,1,31,32,33,63,64,65,255],"fft_components_per_vector":18,
                "vmag_values_per_vector":256,"full_numerical_tables":path.as_posix(),"tolerance":reference['tolerance']}
        manifest['units'].append(entry)
    for entry in manifest['units']:
        ident=entry['id']
        if ident.startswith('CFX'):
            entry['auxiliary_scenarios']=[
                {'key':'independent-repeat-restore','source':'tests/unit/tdc_ut_cfx_io_check.py: MAIN',
                 'mode':'PC / RAM / 독립 재실행 2회',
                 'expected_fields':['run=0','done=1','overflow=0','known mismatch count unchanged','all seeded buffers and shared state restored','record header/bounds/count valid']},
                {'key':'undrained-result-busy','source':'tests/unit/tdc_ut_cfx_io_check.py: MAIN',
                 'mode':'PC / RAM / 이전 결과 미해제',
                 'expected_fields':['run_unit(7)=-2','hear_start(1,1,1)=-2','actual[0..767] unchanged','release=0']},
                {'key':'cross-runner-busy','source':'tests/unit/tdc_ut_cfx_io_check.py: MAIN',
                 'mode':'PC / RAM / pure_done, HEAR_pending, HEAR_done 각각 별도 초기화',
                 'expected_fields':['run_unit(7)=-2','hear_start(1,1,1)=-2','actual/expected[0..767] unchanged','shared state unchanged','both release=-2 when HEAR pending']},
                {'key':'unsupported-14-99','source':'tests/unit/tdc_ut_cfx_io_check.py: MAIN','mode':'PC / RAM',
                 'expected_fields':['run_unit(14)=-1','run_unit(99)=-1']},
                {'key':'no-host-HEAR','source':'tests/unit/tdc_ut_cfx_io_check.py: MAIN','mode':'PC / 실행 거부',
                 'expected_fields':['start(1,1,0)=-4','start(1,1,1)=-4','start(2,1,1)=-4','poll=-4','done=0','target_needed=1','vmag_count=0']},
            ]
            if ident in ('CFX-011','CFX-033'):
                entry['auxiliary_scenarios'] += [
                    {'key':'distinct-initial-state-full-restore','source':EXTRA,'mode':'PC / RAM / 별도 비영 초기 상태',
                     'expected_fields':['ordinary units11/33 twice; complete host_state equality','host-only cases100..111 twice; complete state restored']},
                    {'key':'override-busy','source':EXTRA,'mode':'PC / RAM / IO,power,AGC 각각 단독 설정',
                     'expected_fields':['run_unit(11)=-2','run_unit(33)=-2','complete product state and actual/expected[0..767] unchanged']},
                    {'key':'PCM-pointer-outside','source':EXTRA,'mode':'PC / RAM / pointer=&buffer[48],value123',
                     'expected_fields':['overflow=1','writes=0','destination unchanged']},
                    {'key':'PCM-trace-full','source':EXTRA,'mode':'PC / RAM / writes128,pointer=&buffer[47],value123',
                     'expected_fields':['overflow=1','writes=128','destination unchanged']},
                    {'key':'result-capacity-full','source':EXTRA,'mode':'PC / RAM / count768,put(123,123)',
                     'expected_fields':['overflow=1','count=768','no out-of-bounds write']},
                ]
        else:
            helper='tests/unit/tdc_ut_hear_reference_check.py: MAIN'
            entry['auxiliary_scenarios']=[
                {'key':'unsupported-unit-vector-analysis','source':RUNNER,'mode':'분석 / 실행했다고 주장하지 않음',
                 'expected_fields':['unit outside1/2 or vector outside supported range: -1','target poll without pending: -1']},
            ]
            if ident=='HEAR-001':
                entry['auxiliary_scenarios'] += [
                    {'key':'synthetic-final','source':helper,'mode':'PC / 합성 RAM / Event0',
                     'expected_fields':['absolute[0..4]=[100,32,17,100,99];[5..15]=0','chain0 requested','poll=0','failed=0','count=22','all seeded memories restored']},
                    {'key':'intermediate-instead-of-final','source':helper,'mode':'PC / 오류 주입 / absolute[0]=0',
                     'expected_fields':['poll=0','failed=1','all seeded memories restored']},
                    {'key':'start-while-another-HEAR-pending','source':helper,'mode':'PC / 합성 timeout 상태',
                     'expected_fields':['start(1,1,1)=-2','pending memory retained']},
                ]
            else:
                scenarios=[
                    ('comparison-boundaries','FFT ref1000/raw1064,1065,1066; vMag ref1000/raw537,538', ['true,true,false','true,false']),
                    ('representation-rejection','reference0/E0: vMag raw-1 or FFT raw8388608; FFT raw0/reference0: E-1 or E14',['false for each']),
                    ('wrong-window','window[123]=1;start(2,2,1)',['-6','pending=0','no chain requested','work memories unchanged']),
                    ('old-event','chain1=1 before start',['-2','old event preserved','no chain requested']),
                    ('synthetic-five-vectors','vectors1..5, E4; selected FFT=round(R/16),vmag=round(R/32)',['poll=0','failed=0','FFT512/vmag256/E4','all seeded buffers restored','record counts8,282,282,282,282']),
                    ('half-FFT','vector2/E4 fft_output[65]/=2',['poll=0','failed>0','restored']),
                    ('double-vMag','vector2/E4 vmag[32]*=2',['poll=0','failed>0','restored']),
                    ('side-bin-corruption','vector2/E4 vmag[17]=10000',['poll=0','failed>0','restored']),
                    ('negative-vMag','vector2/E4 vmag[17]=-1',['poll=0','failed>0','restored']),
                    ('invalid-exponent','vector2/E4 change E to14',['poll=0','failed>0','restored']),
                    ('nonzero-zero-input','vector1/E4 fft_output[223]=1',['poll=0','failed=1','restored']),
                    ('pending-timeout-late','vector2 start; no event polls1..100000; release/start; then synthetic event',
                     ['poll1..99999=1','poll100000=-5','pending1/done0/timed_out1','input[0..1]=[0,25080] retained','release=-2/new start=-2','late poll=0 but failed1/timed_out1','case99 actual1/expected0','restore only after completion']),
                ]
                entry['auxiliary_scenarios'] += [{'key':key,'source':helper,'mode':'PC / 합성 RAM / '+inp,'expected_fields':expected}
                                                 for key,inp,expected in scenarios]
    manifest['checks'].append({"key":"document_case_inventory","status":"PASS","units":14,
                               "cases":sum(len(u['cases']) for u in manifest['units']),
                               "CFX_regular_cases":49,"CFX_PC_extra_cases":12,"HEAR_vectors":6,
                               "auxiliary_scenarios_by_unit":{u['id']:len(u['auxiliary_scenarios']) for u in manifest['units']},
                               "meaning":"중복 실행/최적화와 별개의 본문 case 수. 공통 IO 보호 시나리오는 각 유닛 적용 항목이며 전역 고유 수로 합산하지 않음"})
    files.append({"path":MANIFEST.as_posix(),"content":json.dumps(manifest,ensure_ascii=False,indent=2)+"\n"})
    return files,manifest


def check_files(files:list[dict]) -> dict:
    links=0
    summaries=0
    for item in files:
        path=ROOT/item['path']
        assert path.is_file(),f"누락: {path}"
        actual=path.read_text(encoding='utf-8')
        assert actual==item['content'],f"저장 내용 불일치: {path}"
        if path.suffix=='.md':
            assert actual.startswith('# '+path.stem+' ')
            assert '\ufffd' not in actual
            assert '## 근거 소스와 관찰 자료' in actual
            assert '주 작업' not in actual and '서브에이전트' not in actual
            assert actual.count('```')%2==0
            table_width=None
            for line in actual.splitlines():
                if line.startswith('|'):
                    width=len(re.split(r'(?<!\\)\|',line))
                    assert table_width in (None,width), f"표 열 수 {path}: {line}"
                    table_width=width
                else:
                    table_width=None
            summaries+=len(re.findall(r'^\[UT-(?:CFX|HEAR)-\d{3}\] C\d+ PASS N=\d+ F=0$',actual,re.M))
            for target in re.findall(r'\]\(([^)]+)\)',actual):
                assert (path.parent/unquote(target)).resolve().exists(), f"깨진 링크 {path}: {target}"
                links+=1
    assert summaries==55
    return {"status":"PASS","documents":14,"manifest":MANIFEST.as_posix(),"verified_links":links,
            "RTT_pass_expected_summary_lines":summaries,"markdown_tables_and_fences":"PASS",
            "product_tests_rerun":False,"device_execution":False,"baseline_validation":"main 소유 / 여기서 미수행"}


def cfx_extra_log_lines(unit: int, vector: int, expected: list[int]) -> list[str]:
    """host_finish의 두 정상 stdout 줄. 관찰값 없이 사전 기대 배열만 쓴다."""
    values = ','.join(str(value) for value in expected)
    return [f'CFX_EXTRA {{"unit":{unit},"case":{vector},"repeat":{repeat},'
            f'"actual":[{values}],"expected":[{values}]}}' for repeat in (0, 1)]


def make_log_export(units: dict[int, list[dict]], hear: dict[int, list[dict]], reference: dict) -> dict:
    """관찰 자료를 받지 않고 독립 기대값과 고정 소스 계약만 직렬화한다."""
    result = {
        'role': 'cfx-io-hear',
        'schema_version': 1,
        'units': [],
        'log_types': {
            'rtt_compare': {
                'source': [REPORT, LOG],
                'input': 'expected는 머리말 없는 payload 전량. 통과 전문에는 A=expected와 E=expected를 포매터에 전달한다.',
                'status': 'PASS',
                'notes': '사전 통과 예상 전문이며 현재 제품이나 실장치의 통과 기록이 아님. C/N을 보존한다.'},
            'rtt_timeout_failure': {
                'source': RUNNER + ': tdc_ut_hear_poll; mark(unit,99,1); put(1,0)',
                'input': 'expected=[0], emitted_values=[1]은 소스의 고정 실패 주입값. PASS 배열로 대체하지 않는다.',
                'status': 'FAIL'},
            'rtt_observed': {
                'source': [REPORT + ': tdc_ut_report_cfx_values_begin', LOG],
                'input': 'expected는 수학적 사전 값만 뜻하며 reporter에는 expected=NULL로 전달한다.',
                'status': 'OBSERVED',
                'notes': '영 입력의 raw 전량0도 OBSERVED N=... F=0. 이 F=0은 비교 성공이 아님.'},
            'CFX_EXTRA': {
                'source': EXTRA + ': host_finish',
                'prefix': 'CFX_EXTRA ',
                'json_field_order': ['unit', 'case', 'repeat', 'actual', 'expected'],
                'json_separators': [',', ':'],
                'repeat_values': [0, 1],
                'text_fields': 'log_lines는 repeat 0/1 순서의 완전한 두 줄(종결 개행 제외). expected_text는 각 줄 뒤에 LF를 붙인 전문.',
                'line_ending': '\n',
                'input': '각 repeat의 정상 출력은 actual과 expected에 같은 사전 expected 전량을 넣는다. 머리말 4워드 없음.',
                'status': None,
                'notes': 'PC JSON 한 줄이며 RTT A/PASS 형식 없음. 특성화 사례의 배열 일치는 안전 정책 PASS가 아님.'},
            'literal_stdout': {'input': 'text 그대로 PC stdout에 출력. A배열 또는 RTT PASS 행을 만들지 않는다.'},
        },
        'transport_notes': [
            'RTT 줄분할은 src/test/tdc_ut_log.c를 사용한다. 이 export는 RTT 전문을 만들지 않으며 PC CFX_EXTRA stdout만 소스 형식으로 제공한다.',
            '주소는 로그 A배열 원소가 아니다. actual_word/expected_word/raw word 주소는 현재 빌드의 XMEM XRAM 심볼에서 읽는다. 고정 주소를 예측하지 않는다.',
            'records의 condition/source_vector/log_type을 함께 읽는다. C80/C81은 source_vector=1일 때만 결정적 원시값을 갖는다.',
            'O0/O2 및 재실행은 같은 벡터를 사용한다. PC CFX_EXTRA의 repeat 0/1은 별도 벡터로 세지 않는다.',
        ],
    }
    for core, groups in [('CFX', units), ('HEAR', hear)]:
        for unit, cases in groups.items():
            ident = f'{core}-{unit:03d}'
            entry = {'id': ident, 'file': (OUT/core/f'{ident}.md').as_posix(),
                     'records': [], 'variable_records': [], 'pc_only_records': []}
            for c in cases:
                is_pc = c['extra']
                record = {
                    'case': int(c['key']), 'expected': payload(c),
                    'mode': 'pc' if is_pc else 'target',
                    'log_type': 'CFX_EXTRA' if is_pc else 'rtt_compare',
                    'expected_status': None if is_pc else 'PASS',
                    'source': c['source'], 'title': c['title'],
                    'condition': c['input_and_action'],
                    'notes': c['mode'] + '. ' + c['limitation'],
                }
                if core == 'CFX':
                    record['notes'] += ' ' + cfx_notes(unit)
                else:
                    record['notes'] += ' 실제 가속기 미실행. 조건을 만족할 때 비교 배열의 사전값이며 raw BFP 값을 복사한 배열이 아니다.'
                entry['records'].append(record)
                if is_pc:
                    record['repeat_values'] = [0, 1]
                    record['log_lines'] = cfx_extra_log_lines(unit, int(c['key']), record['expected'])
                    record['expected_text'] = ''.join(line+'\n' for line in record['log_lines'])
                    entry['pc_only_records'].append({
                        'case': int(c['key']), 'log_type': 'CFX_EXTRA',
                        'expected_record_case': int(c['key']), 'repeat_values': [0, 1],
                        'notes': '전체 배열은 records의 동일 case/mode=pc를 사용. 이 추가 사례를 타깃 RTT로 출력하는 경로는 없음.'})
            if core == 'HEAR':
                entry['records'].append({
                    'case': 99, 'expected': [0], 'emitted_values': [1], 'mode': 'target',
                    'log_type': 'rtt_timeout_failure', 'expected_status': 'FAIL',
                    'source': RUNNER + ': tdc_ut_hear_poll',
                    'condition': '100000회 미완료 poll 이후 실제 완료가 늦게 도착한 경우에만 추가한다.',
                    'notes': '소스의 put(1,0)으로 항상 F=1. 일반 벡터가 일치해도 이 기록은 FAIL이며 PASS 전문을 생성하지 않는다.'})
                entry['pc_only_records'].append({
                    'case': None, 'log_type': 'literal_stdout',
                    'text': 'HEAR_COMPARATOR_LIFECYCLE_OK: synthetic comparison inputs only; accelerator NOT executed',
                    'shared_line_id': 'hear-comparator-lifecycle',
                    'source': 'tests/unit/tdc_ut_hear_reference_check.py: MAIN',
                    'notes': 'HEAR1/2 합성 검사 전체의 단일 종료행. 개별 오류 주입/복원 사례의 RTT 로그가 아님. 두 문서에 소개되어도 실제 한 번 출력.'})
            if core == 'HEAR' and unit == 2:
                for key, count, name in [(80, 512, 'FFT'), (81, 256, 'vMag')]:
                    entry['records'].append({
                        'case': key, 'expected': [0]*count, 'mode': 'target',
                        'log_type': 'rtt_observed', 'expected_status': 'OBSERVED',
                        'reporter_expected_is_null': True, 'source_vector': 1,
                        'source': RUNNER + ': zero-input fft_any/vmag_any; ' + REPORT,
                        'condition': 'vector 1 영 입력이 정확한 0 출력 조건을 만족한 뒤 raw 관찰을 배출하는 경우만 해당.',
                        'notes': f'{name} 전체{count}워드 0은 OR 비교의 사전 기준. raw reporter는 expected=NULL이므로 PASS가 아닌 OBSERVED. vector2..5에는 이 배열을 사용하지 않는다.'})
                for key, name, count, reference_key in [
                    (80, 'FFT raw', 512, 'fft_unscaled_packed_re_im'),
                    (81, 'vMag raw', 256, 'vmag_unscaled'),
                ]:
                    values = []
                    for vector in reference['vectors'][1:]:
                        refs = vector[reference_key]
                        values.append({'source_vector': vector['vector'], 'reference': refs,
                                       'absolute_tolerance': [64+(abs(v)+999)//1000 if key == 80 else
                                                              64+(v+99)//100 for v in refs]})
                    field_info = {
                        'name': f'{name}[0..{count-1}]', 'index_range': [0, count-1],
                        'raw_range': [-8388608, 8388607] if key == 80 else [0, 8388607],
                        'variable_reason': '실제 BFP 블록 지수 E와 정수 반올림/하드웨어 연산 오차에 따라 raw 값이 달라진다. E=4는 PC 합성 입력일 뿐 장치 기대값이 아니다.',
                        'comparison': 'abs(raw*2^E-R)<=tolerance' if key == 80 else 'abs(raw*2^(E+1)-R)<=tolerance',
                        'reference_by_vector': values,
                    }
                    if key == 80:
                        field_info['compared_raw_indices'] = [2*k+c for k in reference['vectors'][0]['fft_reference_bins'] for c in [0, 1]]
                        field_info['unchecked_values'] = '512워드 중 위 18개 성분 밖은 캡처만 하며 수치 자동 비교 범위가 아님. raw[1]은 Re(X[256]).'
                    entry['variable_records'].append({
                        'case': key, 'mode': 'target', 'log_type': 'rtt_observed',
                        'source_vectors': [2, 3, 4, 5], 'expected_status': 'OBSERVED',
                        'fields': [field_info],
                        'notes': 'A줄 전체의 단일 정수열은 예측 불가. 별도 비교 레코드 C2..C5의 Boolean 1 배열과 혼동하지 않는다.'})
                entry['variable_records'].append({
                    'case': 82, 'mode': 'target', 'log_type': 'rtt_observed',
                    'source_vectors': [1, 2, 3, 4, 5], 'expected_status': 'OBSERVED',
                    'fields': [{'name': 'g_tdc_ut_hear_block_exp', 'index_range': [0, 0],
                                'range': 'vector2..5: 0..13; vector1: 영 입력의 정확한0 판정에 E 범위 비교 없음',
                                'variable_reason': '실제 HEAR BFP 지수. 합성 fixture의 E=4를 장치 기대값으로 고정하지 않는다.'}],
                    'notes': 'N=1, expected=NULL이므로 OBSERVED F=0. 주소와 달리 E 자체는 A[0:0]의 관찰값으로 출력된다.'})
            result['units'].append(entry)
    return result


def build_log_export() -> dict:
    """기존 문서 생성/검증 산출물은 바꾸지 않고 로그용 배열을 검증한다."""
    units = cfx_cases()
    extra_cases(units)
    reference = read_json(REF)
    hear = hear_cases(reference)
    evidence = read_json(EVIDENCE)
    window = read_json('tests/unit/tdc_ut_cfx_io_criteria.json')['HEAR-002']['exact_window_512']
    checks = validate_expectations(units, evidence, reference, window)
    result = make_log_export(units, hear, reference)
    extra_source = (ROOT/EXTRA).read_text(encoding='utf-8')
    assert r'printf("CFX_EXTRA {\"unit\":%d,\"case\":%d,\"repeat\":%d,\"actual\":[",unit,vector,repeat);' in extra_source
    assert r'printf("],\"expected\":[");' in extra_source
    assert r'printf("%s%d",i ? "," : "",g_tdc_ut_cfx_io_actual[i]);' in extra_source
    assert r'printf("%s%d",i ? "," : "",g_tdc_ut_cfx_io_expected[i]);' in extra_source
    assert 'puts("]}");' in extra_source
    assert 'for (int repeat=0; repeat<2; repeat++) for (int vector=0; vector<12; vector++)' in extra_source
    expected_total = 0
    pc_lines = pc_expected_words = 0
    counts = {'rtt_compare': 0, 'CFX_EXTRA': 0, 'rtt_timeout_failure': 0, 'rtt_observed': 0}
    for entry in result['units']:
        core, number = entry['id'].split('-')
        cases = units[int(number)] if core == 'CFX' else hear[int(number)]
        normal = [r for r in entry['records'] if r['log_type'] in ('rtt_compare', 'CFX_EXTRA')]
        assert [r['case'] for r in normal] == [int(c['key']) for c in cases]
        for record, original in zip(normal, cases):
            assert record['expected'] == payload(original)
        for record in entry['records']:
            assert record['expected'] and all(type(v) is int for v in record['expected'])
            counts[record['log_type']] += 1
            expected_total += len(record['expected'])
            if record['log_type'] == 'CFX_EXTRA':
                assert record['mode'] == 'pc' and record['repeat_values'] == [0, 1]
                assert len(record['log_lines']) == 2
                assert record['expected_text'] == '\n'.join(record['log_lines'])+'\n'
                for repeat, line in enumerate(record['log_lines']):
                    assert line.startswith('CFX_EXTRA ') and '\n' not in line and '\r' not in line
                    decoded = json.loads(line.removeprefix('CFX_EXTRA '))
                    assert list(decoded) == ['unit', 'case', 'repeat', 'actual', 'expected']
                    assert (decoded['unit'], decoded['case'], decoded['repeat']) == (int(number), record['case'], repeat)
                    assert decoded['actual'] == decoded['expected'] == record['expected']
                    assert line == 'CFX_EXTRA '+json.dumps(decoded,separators=(',', ':'))
                    pc_lines += 1
                    pc_expected_words += len(decoded['expected'])
            if record['log_type'] == 'rtt_timeout_failure':
                assert record['emitted_values'] == [1] and record['expected'] == [0]
                assert '{ mark(s_tdc_ut_hear_unit,99,1); put(1,0); }' in (ROOT/RUNNER).read_text(encoding='utf-8')
            if record['log_type'] == 'rtt_observed':
                assert record['source_vector'] == 1 and not any(record['expected'])
                assert record['expected_status'] == 'OBSERVED' and record['reporter_expected_is_null']
    assert counts == {'rtt_compare': 55, 'CFX_EXTRA': 12, 'rtt_timeout_failure': 2, 'rtt_observed': 2}
    tree = ast.parse(Path(__file__).read_text(encoding='utf-8'))
    exported = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'make_log_export')
    assert not any(isinstance(n, ast.Name) and n.id in ('evidence', 'captures', 'actual_records', 'read_json', 'records') for n in ast.walk(exported))
    assert not any(isinstance(n, ast.Subscript) and isinstance(n.slice, ast.Constant) and n.slice.value == 'actual' for n in ast.walk(exported))
    formatter = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'cfx_extra_log_lines')
    assert not any(isinstance(n, ast.Name) and n.id in ('evidence', 'captures', 'actual_records', 'read_json', 'records') for n in ast.walk(formatter))
    assert not any(isinstance(n, ast.Subscript) and isinstance(n.slice, ast.Constant) and n.slice.value == 'actual' for n in ast.walk(formatter))
    assert pc_lines == 24 and pc_expected_words == 12082
    checks.append({'key': 'CFX_EXTRA_exact_stdout', 'status': 'PASS',
                   'records': 12, 'repeat_values': [0, 1], 'complete_stdout_lines': pc_lines,
                   'expected_array_words_across_repeats': pc_expected_words,
                   'source': EXTRA + ': host_finish / tdc_ut_cfx_io_host_cases',
                   'method': 'C printf/puts 문자열 및 repeat 루프 정적 대조, JSON 필드 순서/구분자/전체 배열/숫자/repeat/최종 LF 검증',
                   'C_test_executed': False, 'actual_used_to_generate_expected': False})
    checks.append({'key': 'raw_log_export_inventory_and_independence', 'status': 'PASS',
                   'units': len(result['units']), 'record_counts_by_type': counts,
                   'all_expected_integer_values': expected_total,
                   'variable_records': sum(len(u['variable_records']) for u in result['units']),
                   'method': '기존 독립 builder와 각 레코드 전량/순서 대조; make_log_export AST의 관찰 접근 차단; timeout은 소스 put(1,0) 확인',
                   'actual_used_to_generate_expected': False})
    result['checks'] = checks
    return result


def main() -> None:
    parser=argparse.ArgumentParser()
    parser.add_argument('--emit',action='store_true')
    parser.add_argument('--select',help='emit할 단일 파일명 또는 manifest')
    parser.add_argument('--stats',action='store_true')
    parser.add_argument('--offset',type=int,default=0)
    parser.add_argument('--size',type=int)
    parser.add_argument('--check',action='store_true')
    parser.add_argument('--logs-export',action='store_true',help='로그용 원시 기대배열만 stdout JSON으로 반환')
    parser.add_argument('--logs-check',action='store_true',help='저장된 로그 export JSON만 읽기 전용 대조')
    args=parser.parse_args()
    if args.logs_export or args.logs_check:
        log_data=build_log_export()
        content=json.dumps(log_data,ensure_ascii=False,indent=2)+"\n"
        if args.logs_check:
            assert (ROOT/LOG_EXPORT).read_text(encoding='utf-8')==content, '로그 export 파일 내용 불일치'
            print(json.dumps({'status':'PASS','file':LOG_EXPORT.as_posix(),'checks':log_data['checks']},ensure_ascii=False,indent=2))
        elif args.stats:
            print(json.dumps([{'path':LOG_EXPORT.as_posix(),'characters':len(content)}],ensure_ascii=False))
        else:
            if args.size is not None:
                assert args.size>0 and args.offset>=0
                content=content[args.offset:args.offset+args.size]
            print(json.dumps({'files':[{'path':LOG_EXPORT.as_posix(),'content':content}]},ensure_ascii=False))
        return
    files,manifest=build()
    if args.stats:
        print(json.dumps([{'path':f['path'],'characters':len(f['content'])} for f in files],ensure_ascii=False))
        return
    if args.emit:
        if args.select:
            files=[f for f in files if Path(f['path']).name==args.select or args.select=='manifest' and f['path']==MANIFEST.as_posix()]
            assert len(files)==1
        if args.size is not None:
            assert len(files)==1 and args.size>0 and args.offset>=0
            files=[dict(files[0],content=files[0]['content'][args.offset:args.offset+args.size])]
        print(json.dumps({"files":files},ensure_ascii=False))
    else:
        result=check_files(files)
        result['checks']=manifest['checks']
        result['files']=[f['path'] for f in files]
        print(json.dumps(result,ensure_ascii=False,indent=2))


if __name__=='__main__':
    main()
