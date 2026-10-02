"""APx용 합성음 생성 및 저장 파일 재검증. 실기 자극 결과를 생성하지 않는다."""

import hashlib
import json
import re
import wave
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[5]
OUT = ROOT / 'docs/참고/인허가/사운드1/APx NofM 합성 파형'
RATE = 48000
PERIOD_SAMPLES = 1536
REPEATS = 32
FREQUENCIES = [125, 156.25, 187.5, 218.75, 250, 312.5, 375, 437.5,
               500, 562.5, 625, 718.75, 812.5, 937.5, 1062.5, 1187.5,
               1343.75, 1531.25, 1718.75, 1906.25, 2156.25, 2406.25,
               2718.75, 3031.25, 3406.25, 3843.75, 4281.25, 4781.25,
               5343.75, 6000, 6718.75, 7500]
GROUPS = [('ch01-16', 1, 16, 'A: 전극 대역 1~16'),
          ('ch09-24', 9, 24, 'B: 전극 대역 9~24'),
          ('ch17-32', 17, 32, 'C: 전극 대역 17~32')]


def check_band_assignments() -> None:
    """현재 소스의 32밴드 표와 주파수-전극 대응을 대조한다."""
    source = (ROOT / 'src/2__cm3/Gen1_5/FS/ci_fft.c').read_text(encoding='utf-8')
    body = source.split('int ci_fft_init_pass_bin_ch_32(')[1]
    values = re.search(r'pass_bin\[Half_FFT_Size\]\s*=\s*\{([^}]+)', body)
    if values is None:
        raise ValueError('32밴드 표를 찾지 못했습니다.')
    bands = [int(x) for x in re.findall(r'-?\d+', values.group(1))]
    assert len(bands) == 256
    for electrode, frequency in enumerate(FREQUENCIES, 1):
        assert bands[int(frequency / 31.25)] == electrode - 1


def make_waveform(name: str, first: int, last: int) -> dict:
    """모노 PCM24, RMS -24 dBFS(1 FS 기준) 합성음을 저장하고 다시 읽는다."""
    frequencies = np.array(FREQUENCIES[first - 1:last])
    t = np.arange(PERIOD_SAMPLES) / RATE
    signal = np.sin(2 * np.pi * frequencies[:, None] * t).sum(axis=0)
    signal *= 10 ** (-24 / 20) / np.sqrt(np.mean(signal ** 2))
    pcm = np.rint(np.tile(signal, REPEATS) * (2 ** 23)).astype(np.int32)
    assert np.max(np.abs(pcm)) < 2 ** 23
    unsigned = pcm & 0xffffff
    payload = np.column_stack((unsigned & 255, (unsigned >> 8) & 255,
                               (unsigned >> 16) & 255)).astype('u1').tobytes()
    path = OUT / (name + '.wav')
    with wave.open(str(path), 'wb') as stream:
        stream.setparams((1, 3, RATE, 0, 'NONE', 'not compressed'))
        stream.writeframes(payload)
    with wave.open(str(path), 'rb') as stream:
        assert (stream.getnchannels(), stream.getsampwidth(), stream.getframerate(),
                stream.getnframes()) == (1, 3, RATE, PERIOD_SAMPLES * REPEATS)
        raw = np.frombuffer(stream.readframes(stream.getnframes()), dtype='u1').reshape(-1, 3)
    decoded = raw[:, 0].astype(np.int32) | (raw[:, 1].astype(np.int32) << 8) | (raw[:, 2].astype(np.int32) << 16)
    decoded = np.where(decoded >= 2 ** 23, decoded - 2 ** 24, decoded)
    assert np.array_equal(decoded, pcm)
    x = decoded / 2 ** 23
    assert np.array_equal(decoded.reshape(REPEATS, PERIOD_SAMPLES),
                          np.tile(decoded[:PERIOD_SAMPLES], (REPEATS, 1)))
    spectrum = np.abs(np.fft.rfft(x)) * 2 / len(x)
    bins = np.rint(frequencies * len(x) / RATE).astype(int)
    amps = spectrum[bins]
    spread = float(20 * np.log10(amps.max() / amps.min()))
    spectrum[bins] = 0
    unwanted = float(20 * np.log10(max(spectrum.max(), 1e-20) / amps.min()))
    rms = float(20 * np.log10(np.sqrt(np.mean(x ** 2))))
    assert abs(rms + 24) < 0.0001 and spread < 0.0001 and unwanted < -100
    return {'file': path.name, 'electrodes': list(range(first, last + 1)),
            'frequencies_hz': frequencies.tolist(), 'relative_level_db': 0,
            'phase_deg': 0, 'sample_rate_hz': RATE, 'samples': len(x),
            'duration_s': len(x) / RATE, 'rms_dbfs_reference_1': rms,
            'peak_dbfs': float(20 * np.log10(np.max(np.abs(x)))),
            'tone_spread_db': spread, 'largest_unwanted_dbc': unwanted,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def main() -> None:
    check_band_assignments()
    assert [first for _, first, _, _ in GROUPS] == [1, 9, 17]
    assert all(last - first + 1 == 16 for _, first, last, _ in GROUPS)
    OUT.mkdir(parents=True, exist_ok=True)
    results = []
    lines = ['# APx 합성 파형 목록', '',
             '16채널씩 묶고 시작점을 8채널씩 이동한 1~16 / 9~24 / 17~32 조합이다. 8~24는 17채널이므로 사용하지 않는다.', '',
             '현재 시험에는 아래 16톤 WAV 3개를 사용한다.', '',
             '생성 파형의 수치 검증 결과다. APx 실재생·음향 출력·전극 출력 실측 결과가 아니다.', '',
             '모든 파일: 48 kHz / 모노 / PCM 24비트 / 1.024초 / 모든 톤 상대 레벨 0 dB·위상 0°.',
             '총 RMS는 1 FS 기준 -24 dBFS로 동일하다. 실제 음압은 기준 마이크로 별도 설정한다.', '',
             '파일 전체를 연속 반복한다. 1ms로 자르지 않는다. 선택 주파수 목록은 기본 32밴드 맵 기준이다.', '',
             '| 파일 | 구분 | 톤 수 | Peak dBFS |', '|---|---|---:|---:|']
    for name, first, last, label in GROUPS:
        result = make_waveform(name, first, last)
        results.append(result)
        lines.append(f'| [{name}.wav]({name}.wav) | {label} | {last-first+1} | {result["peak_dbfs"]:.3f} |')
    lines += ['', "## APx에서 WAV 출력하기\n\n기존 음향 시험 연결인 **APx 아날로그 출력 → 앰프 → 스피커 → Sound1 마이크**를 사용한다. 기준 마이크는 APx 입력에 연결한다. 사람에게 장착하지 않은 시험 장치와 측정용 부하에서 수행한다.\n\n1. **Generator를 OFF**로 두고 기존 음향 시험 프로젝트를 연다.\n2. WAV를 지원하는 측정 화면에서 **Signal Generation → Waveform → Browse for file…**를 선택한다. 이 폴더의 원하는 WAV를 연다. 예: 1~16 대역은 **ch01-16.wav**.\n3. 파일 정보가 **48 kHz / 24 bit / 1 channel**인지 확인하고, 스피커가 연결된 APx 아날로그 출력 채널에 배정한다. APx의 채널 1은 전극 1이 아니라 여러 주파수가 합쳐진 하나의 오디오 출력이다.\n4. **Start Position=0**, 파일 전체 **1.024초(49,152샘플)**를 사용한다. **1ms 구간으로 자르지 않는다.**\n5. 낮은 **Generator Level**에서 **Generator를 ON**으로 전환한다. 기준 마이크에 신호가 들어오는지 확인하고, 파일 전체가 끊김 없이 반복되는지 확인한다.\n6. 기준 마이크의 표시 음압을 보며 목표 음압까지 천천히 조절한다. 기존 **−8.293dB 표시 보정**을 유지한다. 표시70dB SPL은 실제78.293dB SPL에 해당하며, WAV의 −24dBFS나 출력 전압 숫자를 음압으로 해석하지 않는다. 입력·출력 overload가 발생하면 레벨을 낮춘다.\n7. FFT/스펙트럼에서 아래 표의 주파수들이 **동시에** 출력되는지 확인한다. 스피커 응답 때문에 실제 톤별 음압은 달라질 수 있다. 안정된 뒤 여러1ms 구간의 전극 출력을 측정한다.\n8. 다른 조합으로 바꿀 때는 **Generator OFF → 다음 WAV 선택 → 낮은 레벨에서 ON → 음압 재확인** 순서로 진행한다. 종료 시 Generator를 OFF로 둔다.\n\n| 확인할 상황 | 조치 |\n|---|---|\n| Browse for file 메뉴가 없음 | WAV를 지원하는 측정 화면으로 이동한다. 메뉴 위치는 APx 버전에 따라 다를 수 있다. |\n| 마이크 입력이 없음 | Generator ON, 아날로그 출력 채널 배정, 앰프·스피커 연결을 확인한다. |\n| 반복 경계에서 무음·클릭 발생 | 파일 전체 재생과 연속 반복 설정을 확인한 후 측정한다. |\n| 목표 밖 전극도 출력됨 | 16톤을 넣어도 Hann창·AGC·T/C에 따라 다른 전극이 선택될 수 있다. 이를 WAV 오류로 단정하지 않는다. |\n\n**Sound1 기본 맵 3은 NofM·14µs·12슬롯/ms지만 선택 수는16이다.** 기본 맵 1은 CIS·50µs·최대8자극/ms다. 저장된 실제 맵을 확인하고, 16톤 입력이 해당16전극만의 선택을 보장하지 않는다는 점을 유의한다.\n\n출력 메뉴 근거: [APx500 공식 매뉴얼](https://www.ap.com/fileadmin-ap/technical-library/APx500_Users_Manual_v4-4-0.pdf?force=). 자세한 맵·전송 순서와 음압 설명은 [상위 매뉴얼](../APx500%20Sound1%20N-of-M%20시험%20신호%20설정%20매뉴얼.md)을 참고한다. **제공 파일의 수치 검증은 완료했지만 APx 실재생·전극 출력 실측은 미수행이다.**\n"]
    for result in results:
        lines += ['', f'## {result["file"]}', '', '| 전극 번호 | Frequency (Hz) | Level (dB) | Phase (deg) |',
                  '|---:|---:|---:|---:|']
        lines += [f'| {e} | {f:g} | 0 | 0 |' for e, f in zip(result['electrodes'], result['frequencies_hz'])]
    lines += ['', '## 검증', '',
              '3개 WAV를 다시 디코딩하여 길이·PCM 형식·32ms 반복·FFT 주파수·톤별 진폭을 검사했다.',
              '합격 조건: RMS 오차 <0.0001 dB, 톤 간 레벨 차 <0.0001 dB, 비지정 FFT 성분 <-100 dBc.',
              'FFT 검사는 파일 자체에 직사각창을 적용한 결과이며, Sound1의 Hann창 처리 후 채널 분리 검증은 아니다.',
              '상세 수치: [검증 결과](검증%20결과.json). 사용법: [상위 매뉴얼](../APx500%20Sound1%20N-of-M%20시험%20신호%20설정%20매뉴얼.md).', '']
    (OUT / '파형 목록.md').write_text('\n'.join(lines), encoding='utf-8')
    (OUT / '검증 결과.json').write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding='utf-8')
    print(json.dumps([{k: r[k] for k in ('file', 'peak_dbfs', 'tone_spread_db', 'largest_unwanted_dbc')}
                      for r in results], indent=2))


if __name__ == '__main__':
    main()
