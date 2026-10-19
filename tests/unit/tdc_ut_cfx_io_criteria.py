"""Export candidate self-contained test criteria, not a completed test report.

All expected packet words are calculated from declared fixture fields, not
from product output. HEAR references are direct mathematical DFT results.
"""
from __future__ import annotations

import json
from pathlib import Path

from tdc_ut_hear_reference import FFT_BINS, SINE, build

ROOT = Path(__file__).resolve().parents[2]
DESTINATION = ROOT / "tests/unit/tdc_ut_cfx_io_criteria.json"
ORDER = [0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15]
AMPLITUDES = [0, 1, 127, 255] * 8


def packet(phase: int, electrode: int, amplitude: int) -> int:
    return 0x40000 | (phase << 15) | (electrode << 10) | (amplitude << 2)


def block(words: list[int], start: int, pulses: int, stride: int) -> list[int]:
    return [words[(start + i // stride) % len(words)] if i % stride == 0 and i // stride < pulses
            else 0x25555 for i in range(24)]


def main() -> None:
    _, hear = build()
    header_cases = []
    for phase in (0, 1):
        electrodes = [0, 1, 16, 31]
        amplitudes = [0, 1, 127, 255]
        words = [packet(phase, e, a) for e, a in zip(electrodes, amplitudes)]
        header_cases.append({"case": phase + 1, "phase": phase,
                             "assigned_electrodes_1_based": [1, 2, 17, 32],
                             "physical_electrode_map": "map[i]=i", "amplitudes": amplitudes,
                             "expected_header_hex": hex(0x40000 | (phase << 15)),
                             "expected_words": words, "expected_words_hex": [f"0x{word:05X}" for word in words]})
    nofm_cases = []
    for phase in (0, 1):
        selected = ORDER if phase == 0 else [15] + ORDER[:-1]
        words = [packet(phase, ((31 - b) * 7) % 32, AMPLITUDES[b]) for b in selected]
        nofm_cases.append({"case": phase + 1, "first_pulse_phase": phase,
                           "previous_last_band_0_based": 15 if phase == 0 else 0,
                           "expected_output_bands_0_based": selected,
                           "expected_last_band_0_based": 15 if phase == 0 else 7,
                           "expected_packet_words": words, "expected_packet_words_hex": [f"0x{w:05X}" for w in words],
                           "expected_pcm_blocks": [block(words, 0, 8, 3), block(words, 8, 8, 3)],
                           "expected_indices": [8, 0]})
    cis_words = [packet(0, (i * 7) % 32, AMPLITUDES[31 - i]) for i in range(32)]
    specific = [0x11, 0x22, 0x33] + [0x25555] * 21
    specific_full = [0x50000 | (i << 9) | (0x80 + i) for i in range(24)]
    criteria = {
        "scope": "부모가 기준서에 반영할 입력/기댓값 후보. 실기 시험 결과나 보고서 PASS가 아니다.",
        "source_freeze": "2026-10-02; src/1__cfx/test/tdc_ut_io_cfx.c/.h와 생성된 HEAR reference header",
        "method_note": "숫자 비교는 T. HEAR-001 중간값의 microcode/데이터시트 확인은 A. 실기 결과는 실제 실행 후만 기재한다.",
        "preconditions": "오디오/PCM 생산자, 정상 반복 실행, 관련 ISR 소비자 및 CM3 공유값 쓰기를 정지한 뒤 명시적으로 시험을 호출한다. 시험 후 제품 상태를 복원하고 정상 처리를 다시 시작한다.",
        "packet_word_formula": "W=0x40000 | (phase<<15) | (physical_electrode_index<<10) | (Amplitude<<2)",
        "packet_field_expectations": {"bits19_16": "0100 (=4)", "bit15": "입력 phase 0 또는 1", "bits14_10": "물리 전극 인덱스 0..31", "bits9_2": "Amplitude 0..255", "bits1_0": "00 (=0)"},
        "CFX-036": {"input": "4개 대역, CIS 순서 [1,2,3,4], 아래 고정 전극/Amplitude, T 미만 묵음 처리 끔. 2개 위상을 각각 실행한다.",
                    "expected": "헤더, 완성 패킷 단어 및 각 비트 필드가 모두 아래 기댓값과 일치한다. 위상 외 고정 헤더 비트는 가변 API가 아니라 완성 단어의 비트로 확인한다.",
                    "cases": header_cases},
        "CFX-037": {"input_order_0_based": ORDER, "assigned_electrode_1_based_by_band": list(range(32, 0, -1)),
                    "physical_map": "map[i]=(i*7)%32", "amplitude_by_band": AMPLITUDES,
                    "mute_under_T": False, "pcm_frame_stride": 3, "pulses_per_block": 8,
                    "expected": "직전 마지막 대역이 15이면 순서를 유지한다. 직전 마지막 대역이 0이면 선택 목록의 마지막 15만 맨 앞으로 옮긴다. 패킷 16개, NOP 간격 및 출력 인덱스가 모두 일치한다.",
                    "cases": nofm_cases},
        "CFX-033": {"input": "32대역, CIS 순서 32..1, 전극 배정 32..1, map[i]=(i*7)%32, Amplitude=[0,1,127,255] 반복, 위상0, 묵음 끔. 2프레임 간격으로 12개씩 3블록 출력.",
                    "expected_packet_words": cis_words,
                    "expected_pcm_blocks": [block(cis_words, start, 12, 2) for start in (0, 12, 24)],
                    "expected_indices": [12, 24, 4],
                    "limit": "CIS 패킷/분할 출력의 실제 함수 호출. 전략 선택 및 앞단 FFT/로그매핑 전체 연속 실행은 이 IO 벡터의 결과가 아니다."},
        "CFX-040": {"input": "고정 길이 24단어 버퍼. 벡터1은 아래 3데이터+21 NOP. 벡터2는 아래 서로 다른 24단어. 초기 읽기 인덱스17, 읽기 중 플래그0, 요청 모드 SpecificCommand, 다음 모드 NopStandby.",
                    "nop_hex": "0x25555", "nop_decimal": 0x25555,
                    "nop_evidence": ["src/1__cfx/internalDevice/driver_PCM.h: pcm_Mold_NopStandby", "src/2__cm3/source/isd/tdc_isd_pcm.h: pcm_Mold_NopStandby", "src/2__cm3/source/isd/tdc_isd.c: 남은 버퍼 fillSepcificCommndBuffer(i, pcm_Mold_NopStandby)"],
                    "cases": [{"case": 1, "input_and_expected_words": specific}, {"case": 2, "input_and_expected_words": specific_full}],
                    "expected": "각 호출에서 24개 단어를 버퍼 순서대로 출력한다. 쓰기 중 읽기 플래그는 매번1이다. 쓰기 순간 인덱스는 [17,0,1,...,22], 완료 후 인덱스23/플래그0/모드 NopStandby이며 입력 24단어는 바뀌지 않는다.",
                    "length_contract": "길이=3이라는 인자는 없다. reader가 padding을 생성하는 것도 아니다. 호출자가 구성한 24단어 전체를 전송한다.",
                    "concurrency_limit": "읽기 중 플래그와 완료 후 요청 모드 전환을 검증한다. CFX reader의 별도 busy-reject나 CM3 동시 덮어쓰기 방지까지 통과했다고 주장하지 않는다."},
        "CFX-011": {"input": "32대역 T=50/C=150, xmin=0/xmax=2. x=2의 power만 고정하는 기존 시험 override 사용. (자극볼륨,오디오볼륨)=(1,1),(4,10),(4,1),(1,1),(1,1) 순서로 이벤트 함수 호출.",
                    "expected": "Cnew=119,150,150,119,119. T=50 고정, A=8832,12800,12800,8832,8832 및 B=12800. 내부 오디오 볼륨은 0,9,0,0,0. 첫4회 NOP 쓰기24, 마지막 변경 없음은 쓰기0.",
                    "limit": "동시 변경/오디오만 변경/자극만 변경/변경 없음 확인. 하드웨어 설정이 포함된 맵 변경 이벤트는 이 벡터에서 실행하지 않는다."},
        "HEAR-001": {"input": hear["hear1_timing"]["input"],
                     "analysis_intermediate_after_vAbsolute": hear["hear1_timing"]["after_vabsolute_analysis_only"],
                     "execution_expected_after_Event0": hear["hear1_timing"]["after_maxelement_event0_execution_expected"],
                     "expected_flags": {"chain0": 1, "chain1": 0},
                     "criterion": "vAbsolute 중간값은 A로 microcode와 SDK를 대조한다. 실제 CMD0 완료(Event0) 후에는 MaxElement가 word0에 써 놓은 100을 포함한 최종 16단어를 T로 비교한다. 실행 시점을 섞지 않는다."},
        "HEAR-002": {"window_coefficients": hear["input_source"], "window_formula": hear["window_formula"],
                     "exact_window_512": [],
                     "input_sine_period16": SINE,
                     "vector_definitions": ["1: x[n]=0", "2: x[n]=period16[n%16] (bin32)", "3: x[n]=period16[(2*n)%16] (bin64)", "4: x[n]=32768 (DC)", "5: x[n]=32768 for even n, -32768 for odd n (Nyquist)"],
                     "dft_formula": hear["dft_formula"], "representation": hear["representation"],
                     "fft_bins_compared": FFT_BINS, "tolerance": hear["tolerance"],
                     "expected_vectors": [{"vector": v["vector"], "expected_peak_bin": v["peak_bin"],
                                           "fft_unscaled_packed_re_im": v["fft_unscaled_packed_re_im"],
                                           "vmag_unscaled_256": v["vmag_unscaled"]} for v in hear["vectors"]],
                     "zero_vector": "FFT 전체512단어와 vMag 전체256단어가 정확히0인지 비교한다.",
                     "target_observation": "실제 완료 후 g_tdc_ut_hear_fft[512], g_tdc_ut_hear_vmag[256], g_tdc_ut_hear_block_exp를 보존한다.",
                     "runtime_limit": "일반 호스트 시험은 HEAR 실행을 거부(-4)한다. 실기 미실행. 창이 다르면 시작 거부(-6). TIMEOUT(-5) 후에도 pending을 유지하고 실제 완료까지 메모리를 복원하지 않는다. 늦게 완료되더라도 TIMEOUT 실패를 PASS로 바꾸지 않는다."}}
    # Include the input window itself so a workbook can be fully self-contained.
    from tdc_ut_hear_reference import source_window
    criteria["HEAR-002"]["exact_window_512"] = source_window()
    DESTINATION.write_text(json.dumps(criteria, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("CFX_IO_CRITERIA_READY: exact packet words and independent HEAR numerical candidates; NOT device results")


if __name__ == "__main__":
    main()
