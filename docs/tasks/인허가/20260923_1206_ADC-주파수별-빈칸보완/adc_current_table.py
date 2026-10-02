"""Convert the merged ADC grid using the established host DSP model.

Print an apply_patch payload; never modify source data or firmware.
Run from the repository root with Python and NumPy.
"""
import csv
import hashlib
import importlib.util
import io
import json
import re
import sys
from pathlib import Path

import numpy as np

ROOT = Path.cwd()
SOURCE = ROOT / 'docs/참고/인허가/사운드1/ADC 테이블 합체.md'
MODEL = ROOT / 'docs/tasks/인허가/20260914_32채널-시험표/스크립트/simulate.py'
DEST = 'docs/참고/인허가/사운드1/주파수별 예상 자극 전류 테이블.md'
TRACE = 'docs/tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/전류-계산-상세.csv'
META = 'docs/tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/전류-계산-근거.json'
EXPECTED_HASH = '7375f889373223ca663a0e4fce532cfc67833d640479e955ba4adbf6375a9886'


def load():
    assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == EXPECTED_HASH, 'ADC input changed: review before rerun'
    lines = [line for line in SOURCE.read_text(encoding='utf-8').splitlines() if line.startswith('|')]
    freqs = [int(re.sub(r'\D', '', v)) for v in lines[0].split('|')[2:-1]]
    rows = [[int(v.strip().replace(',', '')) for v in line.split('|')[1:-1]] for line in lines[2:]]
    assert freqs == [100,1048,1438,1500,1593,2869,3038,3217,3815,4037,7900]
    assert [r[0] for r in rows] == list(range(29,121))
    assert all(len(r) == 12 and all(0 < v <= 262143 for v in r[1:]) for r in rows)
    spec = importlib.util.spec_from_file_location('legacy_current_model', MODEL)
    model = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(model)
    return freqs, rows, model


def main():
    freqs, rows, model = load()
    used_bins = np.flatnonzero(np.array(model.passbins) >= 0)
    bands = {f: model.passbins[int(used_bins[np.argmin(abs(used_bins*31.25-f))])]+1 for f in freqs}
    assert (500*7185 >> 15) == 109
    assert (109*18677 >> 12) == 497
    assert (1000*6942 >> 15) == 211
    assert 497+(19333*211 >> 12) == 1492
    assert model.current(np.array([0,21892]),1,1)[1].tolist() == [497,1492]
    assert model.current(np.array([0,21892]),10,16)[1].tolist() == [1492,1492]
    values = {}
    detail = io.StringIO(newline='')
    writer = csv.writer(detail, lineterminator='\n')
    writer.writerow(['display_dBSPL','frequency_Hz','ADC_sine_peak','audio_volume','electrode_1based','PCM_min','PCM_center','PCM_max','current_min_uA','current_center_uA','current_max_uA','block_ADC_min','block_ADC_max'])
    cache = {}
    for fidx, f in enumerate(freqs):
        for vol in (1,10):
            for row in rows:
                db, peak = row[0], row[fidx+1]
                key = (f,vol,peak)
                if key not in cache:
                    # Legacy simulate uses adc(db); explicitly supply this cell's ADC.
                    old_adc = model.adc
                    try:
                        model.adc = lambda _db, value=peak: value
                        reps, peaks = model.simulate(f,db,vol)
                    finally:
                        model.adc = old_adc
                    amps, currents = model.current(reps[:,bands[f]-1],vol,bands[f])
                    center = int(np.sort(amps)[(len(amps)-1)//2])
                    triplet = [int(amps.min()), center, int(amps.max())]
                    cur = [497+(19333*a >> 12) for a in triplet]
                    assert all(0 <= a <= 211 for a in triplet)
                    assert currents.min() == cur[0] and currents.max() == cur[2]
                    cache[key] = triplet, cur, peaks
                amp, cur, peaks = cache[key]
                values[vol,db,f] = cur[1]
                writer.writerow([db,f,peak,vol,bands[f],*amp,*cur,*peaks])
        print(f'Calculated {f} Hz / electrode {bands[f]}', file=sys.stderr, flush=True)
    text = ['# 주파수별 예상 자극 전류 테이블', '',
        '기존 [ADC 합체 표](ADC%20테이블%20합체.md)의 29~120 dB SPL·11개 주파수 값을 각각 입력으로 사용한 계산 결과입니다. 각 주파수는 **ADC / 전류 −10% / 예상 전류 / 전류 +10%** 네 열로 구분했습니다. 전류 단위는 µA이며, −10%·+10%는 예상 전류에 각각 0.9·1.1을 곱한 비교 범위입니다. 소수점 한 자리까지 표시하며 DAC 상한으로 절삭하지 않습니다. 이 범위는 실제 시간 변동의 최솟값·최댓값이나 모델의 정확도를 뜻하지 않습니다. 각 열은 해당 주파수를 주로 받는 전극의 펄스 전류입니다. 실측값이나 전체 전극의 합산 전류가 아닙니다.', '',
        '## 적용 조건', '',
        '- 기본 맵 1(CIS), 오디오 볼륨 1 또는 10, 자극 볼륨 4, 펄스폭 50µs.',
        '- 일반 전극 T=500µA / C=1500µA. 정수 DAC 변환을 반영한 계산 범위는 497~1,492µA입니다. 전극 16은 T=C=1500µA로 설정되어 있으나 아래 주파수 열에 해당하지 않습니다.',
        '- 자극 음소거·알림음이 없는 정상 입력 상태를 가정합니다. PCM 진폭 0도 이 조건에서는 약 497µA의 바닥 전류에 해당합니다.',
        '- 표의 ADC를 믹서 출력 순음의 피크 진폭으로 놓았습니다. ADC 원표에는 최대값 기반 값과 평균값 기반 값이 섞여 있어, 특히 블록 최대의 평균을 순음 피크로 보는 오차가 포함될 수 있습니다. 실제 파형이 없으므로 이 표는 조건부 예상값입니다.',
        '- 표시 dB SPL은 원표의 행 이름을 그대로 사용했습니다. 음압 보정·기구 감쇠·빔포밍 감쇠를 다시 적용하지 않았습니다. ADC 상한 262,143은 진폭으로 사용했으며 실제 마이크의 클리핑·왜곡은 모사하지 않았습니다.', '',
        '**표 1. 주파수별 표시 전극(1부터 시작)**', '',
        '| 주파수 Hz | 전극 | FFT 밴드에 포함된 bin 중심 주파수 범위 Hz |',
        '|---:|---:|---:|']
    for f in freqs:
        bins = model.bandbins[bands[f]-1]
        text.append(f'| {f:,} | {bands[f]} | {bins[0]*31.25:g} ~ {bins[-1]*31.25:g} |')
    text += ['', '100Hz와 7900Hz는 각각 사용 FFT bin의 하한(125Hz)·상한(7875Hz) 밖입니다. 가장 가까운 유효 밴드인 전극 1·32에서 Hann 창에 의해 퍼진 성분까지 계산했습니다. 주변 전극에도 성분이 분산될 수 있으므로 이 표를 32채널 전체 출력표로 해석하지 않습니다.', '']
    for table_no, vol in enumerate((1,10),2):
        text += [f'## 오디오 볼륨 {vol}', '', f'**표 {table_no}. 예상 자극 펄스 전류 — 오디오 볼륨 {vol} (µA)**', '',
                 '| 표시 dB SPL | '+' | '.join(f'{f:,} Hz {label}' for f in freqs for label in ('ADC','전류 −10%','예상 전류','전류 +10%'))+' |',
                 '|---:|'+'---:|'*(4*len(freqs))]
        for row in rows:
            text.append('| '+str(row[0])+' | '+' | '.join(f'{row[i+1]:,} | {values[vol,row[0],f]*9/10:,.1f} | {values[vol,row[0],f]:,} | {values[vol,row[0],f]*11/10:,.1f}' for i,f in enumerate(freqs))+' |')
        text.append('')
    text += ['## 계산 방법과 사용 범위', '',
        '각 ADC 값으로 16kHz 순음을 생성하고, 16표본 단위 AGC → 512점 Hann FFT → 밴드별 최대 크기 → 비선형 매핑 → PCM 진폭 → 전류 순서로 계산했습니다. AGC 초기 상태에서 2초가 지난 뒤 1초 동안의 1,000개 결과를 사용했습니다. 표에는 PCM 진폭을 정렬한 가운데 값(짝수 개일 때 아래쪽 값)에 대응하는 전류를 넣었습니다. 순간 최솟값·최댓값은 별도 계산 상세에 보존했습니다.', '',
        '전류 계산식: **I = 497 + floor(19,333 × PCM 진폭 / 4,096) µA**, PCM 진폭은 0~211입니다. 예를 들어 PCM 0은 497µA, PCM 211은 1,492µA입니다. ADC 값에 일정 배율만 곱하는 계산은 아닙니다.', '',
        'AGC와 매핑의 정수 이동·절삭을 반영했지만 DSP 로그/거듭제곱 라이브러리와 HEAR 내부 연산은 이상적인 수학 함수·FFT로 근사했습니다. 따라서 하드웨어와 비트 단위 일치 또는 실측 통과를 보증하지 않습니다. 예상 전류 자체는 변경하지 않고, 비교용 ±10% 범위만 별도 열에 표시했습니다. 저소음 영역의 실측 차이는 이 범위 추가로 해결되거나 검증된 것이 아닙니다. 패킷 한 개 지연은 출력 시점에 영향을 주지만 여기의 펄스 크기는 바꾸지 않습니다.', '',
        '[계산 상세(CSV)](../../../tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/전류-계산-상세.csv) · [재현 스크립트](../../../tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/adc_current_table.py) · [입력·소스 해시](../../../tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/전류-계산-근거.json)', '']
    sources = [SOURCE,MODEL,ROOT/'src/1__cfx/signalProcessing/agc.c',ROOT/'src/1__cfx/signalProcessing/nonlinearMapping.c',ROOT/'src/2__cm3/Gen1_5/FS/ci_map.c',ROOT/'src/2__cm3/Gen1_5/FS/ci_fft.c',ROOT/'src/2__cm3/Cortex-M3-src/signalProcessing/stimulationParaCal.c']
    metadata = dict(input_cells=1012, output_cells=len(values), map=1,audio_volumes=[1,10],stimulation_volume=4,bands=bands,
                    method='Legacy host DSP model; explicit ADC peak; lower median of 1000 PCM values',
                    source_sha256={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sources})
    assert len(values)==2024
    assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==EXPECTED_HASH
    print('*** Begin Patch')
    for path, content in [(DEST,'\n'.join(text)),(TRACE,detail.getvalue()),(META,json.dumps(metadata,ensure_ascii=False,indent=2)+'\n')]:
        print('*** Add File: '+path)
        for line in content.splitlines(): print('+'+line)
    print('*** End Patch')


if __name__ == '__main__':
    main()
