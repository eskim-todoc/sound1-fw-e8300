"""시험 기준 후보와 이상적인 DAS/관측 위상 모델. 제품 실기 시험이 아님.

--document 는 Markdown만 표준 출력한다. 실제 문서 반영은 apply_patch로 한다.
--frequency 로 임의 주파수의 계산 가능한 영향과 조건부 보간을 구분해 본다.
"""
from __future__ import annotations

import argparse
from decimal import Decimal, ROUND_HALF_UP
import json
import math
from statistics import median

from adc_estimate import FREQUENCIES, DISPLAY_LIMIT, prepare, phase_statistics

FS = 16000.0
MIC_DISTANCE = 0.022
SOUND_SPEED = 343.0  # 실측 온도값이 아닌 계산 가정
DELAY_SAMPLES = 1.0 + 6.0 / 240.0
FIT_LEVELS = (55, 60, 65, 70, 75, 80)
_, REFERENCE, _, OBSERVATIONS = prepare()


def db(value: float) -> float:
    return 20.0 * math.log10(max(value, 1e-15))


def beam_gain(frequency: float, angle: float = 0.0) -> float:
    """동일 전달함수 두 마이크·평면파. 0도=선택된 정면, 180도=반대쪽."""
    if not 0 <= frequency <= FS / 2 or not math.isfinite(angle):
        raise ValueError("주파수 0~8000 Hz, 유한한 각도가 필요함")
    residual = DELAY_SAMPLES / FS - MIC_DISTANCE * math.cos(math.radians(angle)) / SOUND_SPEED
    return abs(math.cos(math.pi * frequency * residual))


def continuous_window_worst(frequency: float, count: int = 1600) -> float:
    """연속 원표본 전체를 관측했을 때의 최악 최대값 비율. BLE 폴링 아님."""
    phases = sorted({round((n*2*math.pi*frequency/FS) % math.pi, 12) for n in range(count)})
    gaps = [b-a for a,b in zip(phases,phases[1:])] + [phases[0]+math.pi-phases[-1]]
    return max(0.0, math.cos(max(gaps)/2))


def coefficients() -> dict:
    out = {1048: {"db": 0.0, "median": 0.0, "min": 0.0, "max": 0.0}}
    for frequency, values in OBSERVATIONS.items():
        differences = [db(values[level][0] / REFERENCE[level]) for level in FIT_LEVELS]
        raw = median(differences)
        rounded = float(Decimal(str(raw)).quantize(Decimal("0.1"), rounding=ROUND_HALF_UP))
        out[frequency] = {"db": rounded, "median": raw, "min": min(differences), "max": max(differences)}
    return out


COEFFICIENTS = coefficients()


def center_raw(frequency: int, level: int) -> float:
    return REFERENCE[level] * 10 ** (COEFFICIENTS[frequency]["db"] / 20)


def center(frequency: int, level: int) -> int:
    """포화값으로 잘라 맞추지 않는다. 표시상한을 넘는 값은 별도 경고한다."""
    return math.floor(center_raw(frequency, level) + 0.5)


def limits(frequency: int, level: int) -> tuple[int | None, int | None]:
    return tuple(center(frequency, offset) if offset in REFERENCE else None for offset in (level - 5, level + 5))


def empirical_prediction(frequency: float, level: int) -> dict | None:
    """관측 범위 내 로그 주파수 보간 가설. 미측정 주파수의 합격 기준이 아니다."""
    if not 100 <= frequency <= 7900:
        return None
    lo = max(f for f in FREQUENCIES if f <= frequency)
    hi = min(f for f in FREQUENCIES if f >= frequency)
    t = math.log(frequency / lo) / math.log(hi / lo) if lo != hi else 0.0
    # 실측 계수에 이미 포함된 빔포밍을 이중 적용하지 않는다.
    d_lo = COEFFICIENTS[lo]["db"] - db(beam_gain(lo) / beam_gain(1048))
    d_hi = COEFFICIENTS[hi]["db"] - db(beam_gain(hi) / beam_gain(1048))
    predicted_db = (1 - t) * d_lo + t * d_hi + db(beam_gain(frequency) / beam_gain(1048))
    value = math.floor(REFERENCE[level] * 10 ** (predicted_db / 20) + 0.5)
    return {"adc": value, "bracket_hz": [lo, hi], "db": predicted_db,
            "measured_frequency": lo == hi, "over_display_limit": value > DISPLAY_LIMIT,
            "status": "측정 주파수 기준 후보" if lo == hi else "미측정 주파수 조건부 보간, 판정용 아님"}


def compare_measurements() -> list[dict]:
    result = []
    for frequency, values in OBSERVATIONS.items():
        for level, (peak, mean) in values.items():
            low, high = limits(frequency, level)
            result.append({"frequency": frequency, "level": level, "peak": peak,
                           "low": low, "center": center(frequency, level), "high": high,
                           "inside": low is not None and high is not None and low <= peak <= high,
                           "fit": level in FIT_LEVELS,
                           "delta_db": db(peak / center_raw(frequency, level))})
    return result


def summary() -> dict:
    comparisons = compare_measurements()
    return {
        "fs_hz": FS, "delay_us": DELAY_SAMPLES / FS * 1e6,
        "acoustic_front_us": MIC_DISTANCE / SOUND_SPEED * 1e6,
        "coefficients": COEFFICIENTS,
        "front_7900_db": db(beam_gain(7900)),
        "all_angle_128_worst_db": db(beam_gain(128, 180)),
        "measurement_comparisons": len(comparisons),
        "outside_candidate_limits": [r for r in comparisons if not r["inside"]],
        "fit_count": sum(r["fit"] for r in comparisons),
        "unused_for_fit_count": sum(not r["fit"] for r in comparisons),
        "max_abs_db_residual": max(abs(r["delta_db"]) for r in comparisons),
        "over_limit_centers": [(f, level, center(f, level)) for f in FREQUENCIES
                               for level in REFERENCE if center_raw(f, level) > DISPLAY_LIMIT],
        "phase": {f: phase_statistics(f) for f in FREQUENCIES},
        "continuous_100ms_worst_db": {f: db(continuous_window_worst(f)) for f in FREQUENCIES},
    }


def document() -> str:
    s = summary()
    lines = [
        "# Sound1 주파수별 ADC 시험 기준안과 빔포밍 영향",
        "",
        "**정면 배치에서는 빔포밍 지연합 자체의 감쇠가 거의 없다. 현재 관측된 주파수별 차이는 빔포밍 식 하나로 설명되지 않는다.** 시험 기준은 실측의 들쭉날쭉한 평균을 따라 바꾸지 않고, 1048Hz 중심곡선에 주파수별 대표 배율을 적용하는 안으로 정리했다.",
        "",
        "이 문서는 **시험 기준 후보와 계산 근거**다. 동일 자료에 잘 맞는다는 것과 다른 시료에서도 성립한다는 것은 다르다. 정식 기준서의 합격 기준은 변경하지 않았다. 측정한 최대·평균과 앞서 빈칸을 채운 값은 [기존 ADC 테이블](ADC%20테이블.md)에 그대로 보존했다.",
        "",
        "## 1. 이번 기준안의 적용 조건",
        "",
        "- 사용자 확인: 모든 주파수에서 왼쪽 빔포밍을 켜고, 스피커를 선택된 빔 방향의 정면에 맞춘 동일한 배치였다.",
        "- 음압 좌표는 원표의 APx 표시값을 그대로 사용한다. **APx 보정이 걸려 있다면 보정된 표시 음압이지, 시료 위치의 보정 전 실제 음압과 같은 수치는 아니다.** 과거 보정값을 임의로 다시 더하거나 빼지 않는다. 실제 시험기록에는 적용 보정값·주파수·시료 위치를 함께 고정한다.",
        "- 관측 대상은 AGC 전 `maxAudioInput`이다. 이 값 하나는 16표본, 즉 1ms 블록의 절대값 최대이며 장시간 최대값을 보관하는 기능은 아니다.",
        "- 이번 중심값 후보는 **괄호 안의 관측 최대값**으로 만든다. 괄호 밖 평균과 바꾸어 판정하지 않는다. 1048Hz 열은 기존 기준곡선이지 이번 표에서 새로 측정한 최대·평균 쌍이 아니다.",
        "- 후보 도출에 쓴 공통 범위는 55~80dB다. 저음압·고음압 외삽은 수치를 제시하되, 검증된 제품 성능으로 취급하지 않는다. 시료 수·반복 수·관측 시간의 정보가 부족하므로 생산품 공통 규격 확정에는 별도 재현 확인이 필요하다.",
        "",
        "## 2. 중심값과 허용범위",
        "",
        "### 2.1. 주파수마다 하나의 대표 배율을 사용",
        "",
        "같은 음압의 기존 1048Hz 기준값을 R(L), 해당 주파수의 관측 최대를 P(f,L)로 둔다. 모든 주파수에 자료가 있는 55·60·65·70·75·80dB 여섯 점만 같은 비중으로 사용한다.",
        "",
        "`D(f,L) = 20 × log10[P(f,L) / R(L)]`",
        "",
        "`D(f) = 여섯 D(f,L)의 중앙값을 0.1dB로 반올림`",
        "",
        "`중심 ADC C(f,L) = R(L) × 10^[D(f)/20]` (마지막에 정수 반올림)",
        "",
        "중앙값은 한두 관측의 흔들림에 덜 좌우된다. 0.1dB 표시는 계산에 쓸 대표값을 정리한 것이며 측정 정확도가 0.1dB라는 뜻은 아니다. 이 계수는 **전체 측정 경로에 대한 경험적 차이**다. 기구만의 감쇠나 빔포밍만의 감쇠로 이름 붙이지 않는다.",
        "",
        "표 1. 1048Hz 기준곡선 대비 주파수별 중심값 후보",
        "",
        "| 주파수 Hz | 6점 중앙값 dB | 채택 후보 dB | 배율 | 원자료 차이의 최소~최대 dB |",
        "|---:|---:|---:|---:|---:|",
    ]
    for f in FREQUENCIES:
        c = COEFFICIENTS[f]
        lines.append(f"| {f:,} | {c['median']:+.3f} | {c['db']:+.1f} | {10**(c['db']/20):.4f} | {c['min']:+.2f} ~ {c['max']:+.2f} |")
    lines += [
        "",
        "1048Hz의 0dB는 정의상 기준이다. 다른 주파수와 동시 측정하여 주파수 응답을 분리했다는 뜻은 아니다. 128Hz와 7900Hz는 음압이 올라갈 때 기준 대비 비율이 줄어드는 경향도 있어, 일정 배율은 우선 사용할 단순 후보이며 음압 독립 특성이 입증된 것은 아니다.",
        "",
        "### 2.2. ±5dB는 곡선의 앞뒤 5dB 행으로 판정",
        "",
        "기존 기준에서 사용한 ‘음압 대응값 ±5dB’ 뜻을 유지한다. 저음압 보정이 있는 R(L)은 완전한 지수곡선이 아니므로 중심 ADC에 무조건 0.5623과 1.7783을 곱하는 방식과는 값이 달라질 수 있다.",
        "",
        "`하한 = C(f,L−5), 중심 = C(f,L), 상한 = C(f,L+5)`",
        "",
        "`하한 ≤ 관측 최대 ADC ≤ 상한`이면 **이 기준안의 수치 범위**에 들어간다. 평균값은 이 범위와 비교하지 않는다. 음원 설정 허용오차를 제품 허용오차에 다시 더해 범위를 넓히지 않는다.",
        "",
        "표 2. 표시 음압 70dB에서의 판정값 예시",
        "",
        "| 주파수 Hz | 하한: 65dB 대응 ADC | 중심: 70dB ADC | 상한: 75dB 대응 ADC | 기존 70dB 관측 최대 |",
        "|---:|---:|---:|---:|---:|",
    ]
    for f in FREQUENCIES:
        lo, hi = limits(f, 70)
        observed = f"{OBSERVATIONS[f][70][0]:,}" if f != 1048 else "별도 실측 없음"
        lines.append(f"| {f:,} | {lo:,} | {center(f,70):,} | {hi:,} | {observed} |")
    lines += [
        "",
        f"기존 관측 최대 {s['measurement_comparisons']}개는 모두 위 방식의 ±5dB 후보 범위에 들어간다. 다만 {s['fit_count']}개는 계수를 구하는 데 사용한 자료이며, 나머지 {s['unused_for_fit_count']}개도 같은 원표의 다른 음압점이다. **독립 시료 검증이나 새로 수행한 제품 합격 시험은 아니다.** 중심값 대비 가장 큰 진폭 차이는 약 {s['max_abs_db_residual']:.2f}dB다.",
        "",
        "### 2.3. 29~120dB 중심값 후보 전체표",
        "",
        "다음 표에서 대상 행의 중심값을 읽고, 그보다 5dB 아래·위 행을 하한·상한으로 읽으면 된다. `참고` 행은 공통 계수 도출 구간 밖이다. 100Hz·114Hz는 일부 참고 행에도 기존 실측이 있지만, 전 주파수에 대한 검증을 뜻하지 않는다.",
        "",
        "29~33dB는 L−5가, 116~120dB는 L+5가 원곡선 범위 밖이다. 이 끝 구간의 양측 ±5dB 한계를 임의 외삽하거나 잘라 정하지 않았다. 정식 기준에는 별도의 끝단 판정 규칙이 필요하다. `초과`는 계산 중심값이 문서 상한 262,143보다 크다는 뜻이다. 이 값을 262,143으로 바꾸어 선형 응답이나 합격을 주장하지 않는다.",
        "",
        "표 3. 중심 ADC 후보 — 괄호 없는 단일 값이며 측정 평균이 아님",
        "",
        "| 표시 dB SPL / 구간 | 100 Hz | 114 Hz | 128 Hz | 1048 Hz | 3038 Hz | 3815 Hz | 4037 Hz | 7900 Hz |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for level in REFERENCE:
        values = [f"{center(f,level):,}" + (" 초과" if center_raw(f,level)>DISPLAY_LIMIT else "") for f in FREQUENCIES]
        region = "공통" if 55 <= level <= 80 else "참고"
        edge = "·끝단" if level < 34 or level > 115 else ""
        lines.append(f"| {level} / {region}{edge} | " + " | ".join(values) + " |")
    lines += [
        "",
        "3038Hz는 대표값이 기준보다 +1.8dB 높아 119·120dB 계산 중심이 표시상한을 넘는다. 그보다 낮은 입력에서도 L+5 상한이 표시상한을 넘을 수 있다. 이런 구간은 ‘큰 ADC가 나오면 정상’으로 끝낼 수 없으며 포화·왜곡 여부를 따로 확인해야 한다. 여기의 262,143은 기존 문서의 기준상한이며, 마이크가 모든 주파수의 120dB 입력에서 반드시 정확히 이 값을 출력한다는 보증값으로 사용하지 않는다.",
        "",
        "## 3. 빔포밍 감쇠는 수식으로 계산 가능",
        "",
        "### 3.1. 현재 펌웨어에 대응하는 식",
        "",
        "현재 경로는 16kHz, 2마이크 평균형 지연합이다. 각 입력을 `>>5`로 줄인 뒤, 전방 마이크에 총 1.025표본 지연을 주어 합하고 `>>1`로 두 채널 평균을 낸다. 같은 크기로 정렬된 두 입력은 더해진 뒤 2로 나누므로 **단일 마이크 대비 무조건 −6dB가 되는 구조가 아니다.** 한쪽 입력이 실제로 기여하지 않는다면 그때는 절반이 될 수 있다.",
        "",
        "코드에 기록된 마이크 간격 d=22mm, 계산용 음속 c=343m/s를 적용했다. 두 마이크 감도와 기구 전달함수가 같고 평면파인 경우다. 실제 온도·공차·초근접 음장은 이 이상 조건에 포함되지 않는다.",
        "",
        f"`펌웨어 지연 τb = (1 + 6/240) / 16000 = {s['delay_us']:.6f}μs`",
        "",
        f"`정면 음향 도달 시간차 τa = 0.022 / 343 = {s['acoustic_front_us']:.6f}μs`",
        "",
        "`잔여 시간차 δ(θ) = τb − (d/c)cosθ` (선택된 정면 θ=0°)",
        "",
        "`진폭 배율 B(f,θ) = |cos[πfδ(θ)]|`, `감쇠 dB = 20log10 B(f,θ)`",
        "",
        "이는 같은 진폭의 두 정현파를 지연시켜 평균한 `(1 + exp(−j2πfδ))/2`의 크기다. 이상적인 지연합은 도착 시간차를 보상하면 신호가 같은 위상으로 더해진다는 원리를 사용한다. [MathWorks, Beamforming Overview](https://www.mathworks.com/help/phased/ug/beamforming-concepts.html)",
        "",
        "표 4. 계산된 빔포밍 감쇠 — 실측값이 아닌 이상 모델",
        "",
        "| 주파수 Hz | 정면 0° dB | 측면 90° dB | 반대쪽 180° dB |",
        "|---:|---:|---:|---:|",
    ]
    for f in FREQUENCIES:
        lines.append(f"| {f:,} | {db(beam_gain(f)):.6f} | {db(beam_gain(f,90)):.3f} | {db(beam_gain(f,180)):.3f} |")
    lines += [
        "",
        f"정면 7900Hz 감쇠도 {db(beam_gain(7900)):.6f}dB에 불과하다. 특히 128Hz는 이 모델에서 방향을 반대로 두어도 약 {db(beam_gain(128,180)):.3f}dB뿐이다. 실측의 −3~−5dB급 차이를 22mm 지연합의 방향 효과로 설명할 수 없다. 반대로 4kHz 부근은 **반대쪽 입사**라면 깊게 줄어들 수 있으므로, 방향 조건을 바꾸면 별도 기준이 필요하다.",
        "",
        "### 3.2. 수식 밖에 남는 영향",
        "",
        "두 마이크·기구의 복소 전달함수를 H1(f), H2(f), 공통 입력 필터를 F(f)라고 하면 실제 합성 응답은 다음처럼 확장된다. 복소 전달함수란 크기 변화와 도착 위상 변화를 함께 표현한 것이다.",
        "",
        "`H합성 = F(f) × [H1(f) exp(−j2πfτb) + H2(f) exp(−j2πfτa)] / 2`",
        "",
        "현재 표에는 H1·H2의 위상, 개별 마이크 입력, 시료 위치의 음압 분포가 없다. 따라서 동일 정면 배치라는 사실만으로 기구·멤브레인의 영향까지 제거되지는 않는다. 두 포트의 전달함수가 다르면 정면에서도 합산 손실이 생길 수 있다. 이는 ‘설계된 순수 지연합 응답’과 구별해야 한다.",
        "",
        "코드에는 20Hz DC 제거 필터와 0~8kHz 디시메이션 필터도 켜져 있다. 제조사 SDK는 DC 제거가 1차 필터이며 차단주파수는 16kHz 표본화 기준임을 설명한다. 1차 20Hz 고역통과의 연속시간 근사로 보면 100Hz 약 −0.17dB, 128Hz 약 −0.10dB여서 이 차이만으로 저주파 수 dB 손실을 설명하기도 어렵다. 이 근사는 하드웨어의 비트 단위 재현은 아니다. 7900Hz에서는 디시메이션 대역 가장자리 응답도 확인 대상이므로 이를 임의의 0dB로 가정해 전체 응답을 확정하지 않았다.",
        "",
        "`>>5`와 `>>1`은 주파수별 보정 필터가 아니다. 산술 우측 시프트라면 이상적인 1/64 합산값에 대한 정수화 오차는 2 ADC count 미만이다. 다만 저음압에서 그 몇 count의 상대 영향은 커질 수 있고, 잡음이 무조건 전부 사라진다는 뜻도 아니다.",
        "",
        "## 4. 16표본 최대값을 읽는 타이밍은 별도 영향",
        "",
        "정현파의 실제 진폭을 A, 시작 위상을 φ라 하면 읽히는 블록 최대는 `A × p(f,φ)`다.",
        "",
        "`p(f,φ) = max(n=0…15) |sin(2πfn/16000 + φ)|`",
        "",
        "다음 표는 신호 진폭을 100%로 두고 모든 시작 위상을 계산한 것이다. ‘평균’ 열은 **위상이 균등하게 관측된 경우에만** 성립한다. 무선 조회 주기가 특정 위상만 반복하면 실제 평균은 달라진다. 기구 감쇠나 AGC 이득을 계산한 표가 아니다.",
        "",
        "표 5. 16표본 절대값 최대의 위상별 범위와 균등위상 평균",
        "",
        "| 주파수 Hz | 블록 한 번의 최악 / 실제 진폭 | 최선 | 균등위상 평균 / 실제 진폭 |",
        "|---:|---:|---:|---:|",
    ]
    for f in FREQUENCIES:
        p = phase_statistics(f)
        lines.append(f"| {f:,} | {100*p['worst']:.2f}% | 100% | {100*p['uniform_mean']:.2f}% |")
    lines += [
        "",
        "예를 들어 100Hz와 7900Hz는 같은 16표본 위상 격자를 가져 평균 약 79.67%, 최악 약 29.03%다. 1048Hz는 평균 약 99.80%다. 이 차이는 **밖에 적은 평균값이 낮아지고 흔들리는 일부 이유**가 될 수 있다. 기존 실측에 다시 이 계수를 곱하면 관측 손실을 중복 적용하므로 기준안 배율에 추가하지 않는다.",
        "",
        "100ms 간격으로 조회하는 예에서는 100Hz·7900Hz가 같은 위상으로 고정될 수 있다. 오래 관측했다는 사실만으로 최대를 잡았다고 보장할 수 없다. 현재 실제 무선 조회 간격은 확인되지 않았으므로 이 예를 실측 원인으로 단정하지 않는다.",
        "",
        "**따라서 괄호 안 최대값의 주파수별 차이도 곧바로 기구 감쇠로 확정할 수 없다.** 관측 최대가 실제 파형의 최고점을 충분히 잡았는지 먼저 확인해야 한다. 조회 위상이 고정되어 생긴 차이라면 조회 방식을 바꾸었을 때 달라질 수 있으며, 이를 제품 고유의 보정값으로 고정하면 안 된다.",
        "",
        "4000Hz는 표본열의 절대값이 `|sinφ|, |cosφ|`를 반복하므로 충분히 오래 연속 관측해도 진폭 대비 70.71~100%에 머물 수 있다. 정확히 8000Hz에서는 모든 절대값이 `|sinφ|`가 되어 0~100%다. **시간을 늘리면 모든 주파수에서 오차가 사라진다는 설명은 맞지 않는다.** 8kHz 미만의 새 주파수도 위상 격자를 확인해야 한다.",
        "",
        "시험 절차 확정 전에는 조회 간격·관측 시간·실제로 서로 다른 블록을 읽는지를 고정하고, 관측 최대의 반복성을 먼저 확인한다. 가장 확실한 방법은 연속 원표본 또는 매 블록 누적 최대를 취득하는 것이지만, 현재 펌웨어가 그 누적 최대를 제공한다고 가정하지 않는다. 이 작업에서 펌웨어나 무선 프로토콜은 변경하지 않았다.",
        "",
        f"검증용 취득 조건으로는 **연속 100개 블록(100ms, 1600표본)을 빠짐없이 포함한 최대값**을 권장한다. 표 5의 8개 시험 주파수와 정확한 16kHz·순음 조건에서는 최악의 타이밍 손실이 최대 {max(-db(continuous_window_worst(f)) for f in FREQUENCIES):.6f}dB로 계산된다. 무선 조회 100회나 100ms 동안의 드문 스냅샷은 이 조건이 아니다. 이 계산은 잡음·왜곡·기구 감쇠를 없앤다는 뜻도 아니며, 정확히 4000/8000Hz 같은 다른 주파수에 그대로 일반화하지 않는다.",
        "",
        "## 5. 측정하지 않은 주파수의 예측",
        "",
        "**빔포밍 배율과 16표본 관측 오차는 임의의 주파수에서 계산할 수 있다. 그러나 완제품 ADC의 절대값은 기구·마이크·필터 응답까지 알아야 확정할 수 있다.** 현재 자료로는 아래의 조건부 추정까지만 가능하다.",
        "",
        "관측된 주파수 사이의 잔여 응답이 급격하게 꺾이지 않는다고 가정하고, 표 1 계수에서 이상 빔포밍 차이를 뺀 값을 로그 주파수 축에서 선형 보간한다. 마지막에 목표 주파수의 빔포밍 차이를 되돌린다. 이웃 주파수 사이의 공진·골·음압 의존성이 있으면 이 가정은 틀릴 수 있다. 따라서 **아래 추정값은 시험 계획용이지 미측정 주파수의 합격 기준이 아니다.**",
        "",
        "`t = ln(f/f1) / ln(f2/f1)`",
        "",
        "`D잔여(f) = (1−t)D잔여(f1) + tD잔여(f2)`",
        "",
        "`ADC추정(f,L) = R(L) × 10^[D잔여(f)/20] × B(f,0)/B(1048,0)`",
        "",
        "표 6. 미측정 주파수의 계산 예시 — 표시 70dB, 정면",
        "",
        "| 주파수 Hz | 이상 빔포밍 감쇠 dB | 블록 최악 % | 균등위상 평균 % | 조건부 ADC 추정 | 보간 근거 Hz |",
        "|---:|---:|---:|---:|---:|---|",
    ]
    for f in (250, 500, 1000, 2000, 2869, 3217, 4000, 6000, 7968.75):
        p, prediction = phase_statistics(f), empirical_prediction(f, 70)
        value = f"{prediction['adc']:,}" if prediction else "산출하지 않음"
        bracket = f"{prediction['bracket_hz'][0]:,}~{prediction['bracket_hz'][1]:,}" if prediction else "7900Hz 초과: 경험 보간 범위 밖"
        lines.append(f"| {f:,.2f}".replace(".00", "") + f" | {db(beam_gain(f)):.6f} | {100*p['worst']:.2f} | {100*p['uniform_mean']:.2f} | {value} | {bracket} |")
    lines += [
        "",
        "조건부 ADC는 관측 최대가 실제 진폭에 가깝게 잡혔다는 추가 가정이 있다. 특히 정확히 4000Hz에서는 위상 때문에 그보다 작게 읽힐 수 있다. 128~1048Hz, 4037~7900Hz 사이처럼 간격이 큰 구간은 같은 단순 보간이어도 신뢰도가 낮다. 7968.75Hz는 빔포밍·위상 통계만 계산하며, 대역 끝 응답을 임의 외삽하여 ADC 판정값을 만들지 않았다.",
        "",
        "## 6. 기준 확정에 필요한 최소 확인",
        "",
        "1. 같은 APx 보정, 동일 위치·방향·관측 절차로 **별도 조립 시료와 재배치 반복**을 확인한다. 한 시료에 맞는 계수가 전체 Sound1을 대표하는지 확인하는 단계다.",
        "2. 128Hz·7900Hz의 55/70/80dB를 우선 반복하여 음압에 따른 비율 하락이 재현되는지 본다. 재현되면 그 주파수만 음압 의존 모델을 검토하며, 통과를 위해 개별 행을 낮추지는 않는다.",
        "3. 미측정 주파수 예측을 쓰려면 현재 큰 간격의 중간점(예: 250/500/2000/6000Hz)을 추가 확인한다. 보간 오차가 허용 가능한지 먼저 검증한다.",
        "4. 원인 분리가 필요하면 같은 음장에서 두 마이크의 빔포밍 전 입력과 합산 후 입력을 비교한다. 원표의 반복 측정만으로 기구·마이크·디시메이션·공간 음압을 각각 분리할 수는 없다.",
        "",
        "3815Hz·75dB의 원기록 `(954) 23`은 그대로 보존했다. 이번 기준안은 최대954만 사용하므로 평균23의 오기 여부가 계수 계산에는 영향을 주지 않는다.",
        "",
        "## 7. 근거와 계산 재현",
        "",
        "표본화·상수: [lib_audio_in.h](../../../../src/1__cfx/lib_cfx/lib_audio_in.h), [definitionsForAlgorithm.h](../../../../src/1__cfx/signalProcessing/definitionsForAlgorithm.h). 지연합: [audioMixer.c](../../../../src/1__cfx/systemControl/audioMixer.c). 경로 선택: [main.c](../../../../src/1__cfx/systemControl/main.c), [system_control.c](../../../../src/1__cfx/systemControl/system_control.c). AGC 전 공유: [agc.c](../../../../src/1__cfx/signalProcessing/agc.c).",
        "",
        "제조사 로컬 SDK: Ezairo 8300 Hardware Reference의 Decimation Filtering, Digital Microphone Interface, Input Stage Configuration Registers. 20Hz DC 제거·DMIC 분수지연 지원·레지스터 설정의 근거다. 확인한 설명만으로 완제품의 기구 전달함수나 대역 끝의 실제 ADC 응답을 얻었다고 간주하지 않았다.",
        "",
        "원자료와 모델: [입력 원본](../../../tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/ADC%20테이블_입력원본.md), [재현 계산기](../../../tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/adc_frequency_model.py), [독립 수치검사](../../../tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/test_adc_frequency_model.py). 이 문서의 ‘검사’는 원자료 대조와 수치 계산이며 실기 시험 보고가 아니다.",
        "",
        "작업 폴더에서 `python adc_frequency_model.py --summary`로 대표값과 비교 결과를, `python adc_frequency_model.py --frequency 2869 --level 70`으로 임의 주파수의 계산 결과를 확인한다. `python -m unittest -v test_adc_frequency_model`은 독립 정현파·정수화·경계값 검사를 실행한다.",
        "",
    ]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--summary", action="store_true")
    group.add_argument("--document", action="store_true")
    group.add_argument("--frequency", type=float)
    parser.add_argument("--level", type=int, default=70)
    args = parser.parse_args()
    if args.document:
        print(document(), end="")
    elif args.summary:
        print(json.dumps(summary(), ensure_ascii=False, indent=2))
    else:
        if not math.isfinite(args.frequency) or not 0 < args.frequency < FS/2 or args.level not in REFERENCE:
            parser.error("0<f<8000Hz, 29~120dB 정수 음압만 지원")
        result = {"frequency_hz": args.frequency, "display_level_db": args.level,
                  "front_ideal_beamforming_db": db(beam_gain(args.frequency)),
                  "phase_statistics": phase_statistics(args.frequency),
                  "conditional_estimate": empirical_prediction(args.frequency, args.level),
                  "warning": "계산 모델. 미측정 주파수의 합격 기준 또는 실기 결과가 아님."}
        print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
