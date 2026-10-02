"""제공된 기준곡선과 실측 비율로 빈 ADC 칸만 추정한다.

측정하지 않은 값을 실측으로 바꾸지 않는다. --patch는 패치만 출력하며
대상 문서를 직접 쓰지 않는다. 스크립트와 입력 원본으로 계산을 재현한다.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from decimal import Decimal, ROUND_HALF_UP, localcontext
import hashlib
import json
import math
from pathlib import Path
import re
from statistics import median


TASK = Path(__file__).resolve().parent
ROOT = TASK.parents[3]
SOURCE = TASK / "ADC 테이블_입력원본.md"
TARGET = ROOT / "docs/참고/인허가/사운드1/ADC 테이블.md"
FREQUENCIES = (100, 114, 128, 1048, 3038, 3815, 4037, 7900)
DISPLAY_LIMIT = 262143  # 문서 표시상한. 실제 음향 포화 모델이 아님.
EXCLUDED = {(3815, 75, "mean"): "평균23 원기록 확인 필요"}


@dataclass(frozen=True)
class Estimate:
    value: int
    raw: float
    lower: int
    upper: int
    extrapolated: bool
    limited: bool


def parse_table(text: str) -> dict[int, list[str]]:
    """음압 행만 읽으며 구분선·주석 표를 ADC 입력으로 섞지 않는다."""
    result = {}
    for line in text.splitlines():
        if not re.match(r"^\|\s*\d+\s*\|", line):
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) != 9:
            continue
        level = int(cells[0])
        if 29 <= level <= 120:
            if level in result:
                raise ValueError(f"중복 음압 행: {level}")
            result[level] = cells[1:]
    if sorted(result) != list(range(29, 121)):
        raise ValueError("29~120 dB SPL 92행이 필요함")
    return result


def parse_measurement(cell: str) -> tuple[int, int] | None:
    clean = cell.replace("*", "").replace("_", "").replace(",", "")
    if clean == "-":
        return None
    match = re.fullmatch(r"\((\d+)\)\s+(\d+)", clean)
    if not match:
        raise ValueError(f"원 실측 형식 아님: {cell}")
    maximum, mean = map(int, match.groups())
    if not 0 < mean <= maximum:
        raise ValueError(f"평균/최대 계약 오류: {cell}")
    return maximum, mean


def prepare():
    table = parse_table(SOURCE.read_text(encoding="utf-8"))
    reference = {level: int(cells[3].replace(",", "")) for level, cells in table.items()}
    anchors = {}
    observations = {}
    for column, frequency in enumerate(FREQUENCIES):
        if frequency == 1048:
            continue
        observations[frequency] = {}
        for level, cells in table.items():
            observation = parse_measurement(cells[column])
            if observation:
                observations[frequency][level] = observation
        for stat, pos in (("maximum", 0), ("mean", 1)):
            anchors[frequency, stat] = {
                level: values[pos]
                for level, values in observations[frequency].items()
                if (frequency, level, stat) not in EXCLUDED
            }
    return table, reference, anchors, observations


def interpolate(level: int, reference: dict, points: dict) -> Estimate:
    """R(L) 대비 비율을 로그 보간하고, 범위 밖에서는 끝점 비율을 유지한다."""
    levels = sorted(points)
    lower = max((x for x in levels if x <= level), default=levels[0])
    upper = min((x for x in levels if x >= level), default=levels[-1])
    weight = (level - lower) / (upper - lower) if upper != lower else 0.0
    log_lower = math.log(points[lower] / reference[lower])
    log_upper = math.log(points[upper] / reference[upper])
    raw = reference[level] * math.exp((1 - weight) * log_lower + weight * log_upper)
    return Estimate(
        value=min(DISPLAY_LIMIT, math.floor(raw + 0.5)),
        raw=raw,
        lower=lower,
        upper=upper,
        extrapolated=level < levels[0] or level > levels[-1],
        limited=raw > DISPLAY_LIMIT,
    )


def independent_value(level: int, reference: dict, points: dict) -> int:
    """독립 검산: 50자리 Decimal, log10 비율과 10의 거듭제곱을 사용."""
    with localcontext() as context:
        context.prec = 50
        available = sorted(points)
        if level <= available[0]:
            raw = Decimal(reference[level]) * Decimal(points[available[0]]) / Decimal(reference[available[0]])
        elif level >= available[-1]:
            raw = Decimal(reference[level]) * Decimal(points[available[-1]]) / Decimal(reference[available[-1]])
        else:
            for lo, hi in zip(available, available[1:]):
                if lo <= level <= hi:
                    q = Decimal(level - lo) / Decimal(hi - lo)
                    a = (Decimal(points[lo]) / Decimal(reference[lo])).log10()
                    b = (Decimal(points[hi]) / Decimal(reference[hi])).log10()
                    raw = Decimal(reference[level]) * (Decimal(10) ** (a + q * (b - a)))
                    break
        return min(DISPLAY_LIMIT, int(raw.quantize(Decimal(1), rounding=ROUND_HALF_UP)))


def phase_statistics(frequency: int) -> dict:
    """16표본 최대값의 위상 균등 분포. 설명용이며 추가 보정에 사용하지 않는다."""
    phase = sorted({round((n * 2 * math.pi * frequency / 16000) % math.pi, 14) for n in range(16)})
    gaps = [b - a for a, b in zip(phase, phase[1:])] + [phase[0] + math.pi - phase[-1]]
    return {
        "worst": math.cos(max(gaps) / 2),
        "uniform_mean": sum(2 * math.sin(gap / 2) for gap in gaps) / math.pi,
    }


def build_data():
    table, reference, anchors, observations = prepare()
    filled = {}
    counts = {"original_pairs": 0, "interpolated_cells": 0, "extrapolated_cells": 0, "limited_cells": 0}
    for level, cells in table.items():
        result = list(cells)
        for column, frequency in enumerate(FREQUENCIES):
            if frequency == 1048:
                continue
            if level in observations[frequency]:
                counts["original_pairs"] += 1
                continue
            maximum = interpolate(level, reference, anchors[frequency, "maximum"])
            mean = interpolate(level, reference, anchors[frequency, "mean"])
            assert maximum.value == independent_value(level, reference, anchors[frequency, "maximum"])
            assert mean.value == independent_value(level, reference, anchors[frequency, "mean"])
            assert 0 < mean.value <= maximum.value <= DISPLAY_LIMIT, (frequency, level, maximum, mean)
            external = maximum.extrapolated or mean.extrapolated
            limited = maximum.limited or mean.limited
            result[column] = f"({maximum.value:,}) {mean.value:,}"
            counts["extrapolated_cells" if external else "interpolated_cells"] += 1
            counts["limited_cells"] += int(limited)
        filled[level] = result
    return table, reference, anchors, observations, filled, counts


def equivalent_1048_level(adc: float, reference: dict[int, int]) -> float:
    """같은 최대 ADC에 대응하는 기준 음압. 기준 범위 밖은 외삽하지 않는다."""
    points = sorted(reference.items())
    if not math.isfinite(adc) or not points[0][1] <= adc <= points[-1][1]:
        raise ValueError("ADC가 1048Hz 기준곡선 범위 밖임")
    for level, value in points:
        if adc == value:
            return float(level)
    for (lo_level, lo_adc), (hi_level, hi_adc) in zip(points, points[1:]):
        if lo_adc < adc < hi_adc:
            weight = math.log(adc / lo_adc) / math.log(hi_adc / lo_adc)
            return lo_level + weight * (hi_level - lo_level)
    raise ValueError("단조 증가 기준곡선에서 ADC 대응 구간을 찾지 못함")


def annotated_rows(filled: dict, observations: dict, reference: dict) -> dict:
    """주파수별 실측 시작·끝에만 작은 주석을 추가하며 ADC 숫자는 보존한다."""
    rows = {level: list(cells) for level, cells in filled.items()}
    for frequency, data in observations.items():
        column = FREQUENCIES.index(frequency)
        for level, label in ((min(data), "시작"), (max(data), "끝")):
            equivalent = equivalent_1048_level(data[level][0], reference)
            rows[level][column] += f"<br><small>{label} · 1048Hz {equivalent:.1f}dB</small>"
    return rows


def render_document(reference, anchors, observations, filled, counts) -> str:
    """독자가 표만 보고도 실측/추정 구분과 계산 조건을 확인하게 한다."""
    lines = [
        "# Sound1 주파수별 음압·ADC 테이블",
        "",
        "1048Hz 기준곡선을 유지하고, 나머지 주파수의 빈칸을 기존 실측값과의 비율로 채웠다. **새로 채운 값은 실측 결과가 아니라 계산 추정값이다.** 기존 실측값은 그대로 보존했다.",
        "",
        "**시험 판정용 중심값과 허용범위는 [주파수별 시험 기준안 및 빔포밍 분석](ADC%20주파수별%20시험%20기준안%20및%20빔포밍%20분석.md)을 참조한다.** 아래 표는 관측 최대·평균 및 그 보간 자료이며, 평균의 출렁임을 그대로 합격 기준으로 사용하지 않는다. 전 주파수의 왼쪽 빔포밍·선택 방향 정면 동일 배치는 사용자 확인 조건이다.",
        "",
        "## 1. 표 읽는 방법",
        "",
        "각 칸은 **(최대값) 평균값** 순서다. 여기서 평균은 무선으로 관측한 ADC 값들의 평균이며, 오디오 원표본의 부호 있는 평균이나 RMS가 아니다. 1048Hz 열은 제공된 기준값 한 개를 그대로 사용한다.",
        "",
        "- 표 안의 보간·외삽·상한 기호는 생략하고 ADC 숫자만 적었다. 기존 실측은 원래의 굵은 괄호 표기를 유지했으며, 그 밖의 새로 채운 칸은 계산값이다.",
        "- 각 주파수의 **실측 시작·끝 칸에만** 작은 글씨를 붙였다. `1048Hz 36.7dB`는 그 칸의 **최대 ADC**와 같은 크기가 1048Hz 기준곡선의 약 36.7dB에 해당한다는 뜻이다. 해당 주파수에서 실제로 입력한 음압은 맨 왼쪽 행의 값을 읽는다.",
        "- 시작·끝은 실측이 확보된 범위의 경계이지, 그 사이 모든 행이 실측됐거나 제품 성능이 검증됐다는 표시가 아니다. 시작·끝 사이의 빈칸은 보간, 그 바깥은 외삽으로 채웠다.",
        "- 3038Hz의 119·120dB 칸은 계산값이 표시상한 262,143을 넘어 이 값으로 제한했다. 별도 기호는 붙이지 않았지만 실제 최대·평균이 이 값에 도달한다는 뜻은 아니다.",
        "",
        f"빈칸 {counts['interpolated_cells'] + counts['extrapolated_cells']}개를 채웠다(보간 {counts['interpolated_cells']}개, 외삽 {counts['extrapolated_cells']}개). 그중 {counts['limited_cells']}개는 상한 제한이다. 실측 {counts['original_pairs']}쌍과 1048Hz 기준 92개는 변경하지 않았다.",
        "",
        "실측은 100Hz가 35~90dB, 114Hz가 35~80dB, 나머지가 55~80dB에만 있다. **저음압·고음압의 외삽값은 재측정 전 참고용이며, 시험 통과를 입증하는 실측값이나 확정 합격 기준으로 사용하지 않는다.**",
        "",
        "표 1. Sound1 주파수별 ADC 값 — 괄호 안 최대 / 괄호 밖 평균",
        "",
        "| 음압 dB SPL | 100 Hz | 114 Hz | 128 Hz | 1048 Hz 기준 | 3038 Hz | 3815 Hz | 4037 Hz | 7900 Hz |",
        "|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for level, cells in annotated_rows(filled, observations, reference).items():
        lines.append("| " + " | ".join([str(level), *cells]) + " |")
    lines += [
        "",
        "## 2. 계산 방법",
        "",
        "### 2.1. 1048Hz 대비 비율을 사용한 이유",
        "",
        "같은 표시 음압에서 주파수만 바꿨을 때 얼마나 커지거나 작아졌는지를 실측값에서 구했다. 이 비율에는 빔포밍, 기구·멤브레인, 마이크·필터, 스피커·배치, 최대값을 읽는 타이밍의 영향이 함께 들어 있다. 이 자료만으로 각 원인의 감쇠량을 따로 분리할 수는 없다.",
        "",
        "따라서 빔포밍 감쇠나 기구 감쇠를 별도의 고정 dB로 다시 빼지 않았다. 이미 실측에 포함된 감쇠를 두 번 적용하기 때문이다. 3038Hz처럼 1048Hz 기준보다 큰 값도 관측됐으므로, 모든 주파수가 무조건 작아진다고 가정하지 않았다.",
        "",
        "원표의 음압 좌표를 그대로 사용했다. 과거 APx 보정값 −6.29dB 또는 −8.293dB를 새로 더하거나 빼지 않았다. 표 내 측정들이 같은 음압 표시·보정 설정과 같은 조립·방향 조건에서 얻어졌다는 전제다. 조건이 다르면 해당 실측부터 분리해야 한다.",
        "",
        "### 2.2. 평균·최대값을 각각 보간",
        "",
        "L은 음압(dB SPL), R(L)은 같은 행의 1048Hz 기준값, X는 해당 주파수의 실측 최대값 또는 평균값이다. 두 종류를 독립적으로 계산한다.",
        "",
        "실측점에서의 비율은 `k(L) = X(L) / R(L)`이다. 빈 행 L이 두 실측점 L₁과 L₂ 사이에 있으면 다음 식을 쓴다.",
        "",
        "`t = (L − L₁) / (L₂ − L₁)`",
        "",
        "`X추정(L) = R(L) × k(L₁)^(1−t) × k(L₂)^t`",
        "",
        "이는 ADC 값을 단순 산술평균하는 대신, dB에 대응하는 로그 비율을 부드럽게 연결하는 방법이다. 1048Hz 기준곡선의 저음압·중간·고음압 형태를 바탕으로 주파수별 실측 차이를 반영한다. 비율이 일정한 외삽 구간에서는 기준곡선과 같은 기울기를 갖지만, 비율이 바뀌는 보간 구간에서는 기울기도 달라진다. 마지막에 정수로 반올림한다. 정수 표시는 계산 재현을 위한 것이며 ±1 ADC의 예측 정확도를 뜻하지 않는다.",
        "",
        "예: 114Hz·56dB의 1048Hz 기준은 170이다. 55dB에서 최대 143/평균 94, 60dB에서 최대 224/평균 172이고, 1048Hz 기준은 각각 153과 263이다. 따라서 t=0.2로 계산한다.",
        "",
        "- 최대: `170 × (143/153)^0.8 × (224/263)^0.2 ≈ 156`",
        "- 평균: `170 × (94/153)^0.8 × (172/263)^0.2 ≈ 106`",
        "- 표에는 `(156) 106`으로 표시한다.",
        "",
        "### 2.3. 실측 범위 밖은 끝점의 비율 유지",
        "",
        "최저 실측보다 낮으면 최저 실측의 비율을, 최고 실측보다 높으면 최고 실측의 비율을 유지한다. 실측이 없는 곳에서 주파수 응답이 계속 증가하거나 감소한다고 추가 가정하지 않기 위한 선택이다. 다만 잡음 바닥·고음압 압축·음향 포화가 달라지는 경우는 이 방법으로 예측할 수 없다.",
        "",
        "표 2. 외삽에 사용한 끝점과 1048Hz 대비 비율 — 최대 / 평균",
        "",
        "| 주파수 | 아래쪽 기준점 | 아래쪽 비율 | 위쪽 기준점 | 위쪽 비율 |",
        "|---:|---:|---:|---:|---:|",
    ]
    for frequency, data in observations.items():
        lo, hi = min(data), max(data)
        first = " / ".join(f"{x/reference[lo]:.6f}" for x in data[lo])
        last = " / ".join(f"{x/reference[hi]:.6f}" for x in data[hi])
        lines.append(f"| {frequency} Hz | {lo} dB | {first} | {hi} dB | {last} |")
    lines += [
        "",
        "위 표의 비율은 보기 쉽게 소수 6자리로 표시했으며, 실제 계산에는 표 1의 원 실측값/1048Hz 기준값이라는 정확한 비율을 사용했다.",
        "",
        "예: 7900Hz·110dB는 80dB 실측을 기준으로 외삽한다. `R(110)=81,809`, `R(80)=2,487`이므로 최대는 `81,809×1,607/2,487≈52,862`, 평균은 `81,809×1,318/2,487≈43,355`(반올림)다.",
        "",
        "### 2.4. 상한으로 제한한 값",
        "",
        "이번 표에서는 1048Hz 열의 최대 기준값 262,143을 표시상한으로 채택했다. 3038Hz의 119·120dB는 비율 외삽값이 이를 넘어 262,143으로 제한했다. 상한으로 잘랐기 때문에 최대와 평균이 같은 숫자로 보일 수 있지만, 실제 클리핑된 파형의 평균을 계산한 것은 아니다. 포화 전부터 마이크·필터·믹서의 동작이 달라질 수 있으므로 실제 값은 시험으로 확인해야 한다.",
        "",
        "262,143은 이 문서의 표시 규칙이지 `maxAudioInput`의 모든 경로에 대한 정수 절대 한계를 증명한 값이 아니다. 부호 있는 최솟값의 절대값 처리 등에서 생길 수 있는 1카운트 차이를 이번 표에서 새로 규정하지 않았다.",
        "",
        "### 2.5. 시작·끝에 붙인 1048Hz 대응 음압",
        "",
        "해당 칸의 최대 ADC를 X라 한다. 1048Hz 기준곡선에서 X를 사이에 두는 이웃 두 값 R(L₁), R(L₂)를 찾아 로그 ADC 축에서 역산한다.",
        "",
        "`1048Hz 대응 음압 = L₁ + (L₂−L₁) × ln[X/R(L₁)] / ln[R(L₂)/R(L₁)]`",
        "",
        "예를 들어 100Hz의 실측 시작은 35dB이며 최대 ADC는 34다. 1048Hz 기준은 36dB에서 32, 37dB에서 35이므로 ADC 34는 약 36.7dB에 해당한다. 이를 35dB 행·100Hz 칸 아래에 작게 표시했다. 소수 한 자리는 비교를 위한 계산 표시이며 음압 측정 정확도를 뜻하지 않는다. 기준곡선 범위 밖으로는 역산 외삽하지 않는다.",
        "",
        "## 3. 빔포밍·측정 방식 때문에 주의할 점",
        "",
        "CFX는 두 마이크 입력을 각각 `>>5`한 뒤 지연을 맞춰 더하고 2로 나눈다. 따라서 두 신호가 잘 맞으면 이 평균 동작 자체가 무조건 −6dB를 만드는 것은 아니다. 입사 방향이나 주파수에 따라 두 마이크의 위상·크기가 다르면 결과가 달라진다. 이번 표는 그 차이를 해당 조건의 실측 비율로 반영했다.",
        "",
        "HEAR는 믹싱된 16표본의 절대값 중 최대를 구하고, CFX는 AGC를 적용하기 전에 그 값을 `maxAudioInput`으로 공유한다. 따라서 앱이 수집한 값들의 평균은 ‘16표본 최대값들의 평균’이며 순음의 RMS가 아니다. 최대/평균 비율을 √2로 고정하지 않았다.",
        "",
        "참고로 16kHz 표본화에서 100Hz와 7900Hz는 순음 절대값의 위상 진행 방향만 반대여서, 위상이 균등하면 이상적인 16표본 최대값의 통계가 같다. 이 가정에서 두 주파수의 평균은 참 진폭의 약 79.67%, 1048Hz는 약 99.80%다. 그러나 무선 읽기 타이밍이 특정 위상에 치우치면 이 평균도 달라진다. 이 이론값을 실측에 추가 곱하지 않았으며 실제 평균을 보장하는 수치로 사용하지 않는다.",
        "",
        "기구의 고유한 주파수별 감쇠만 얻으려면 같은 시료·위치·방향·음압에서 조립 전후를 따로 측정해야 한다. 이번 표의 비율을 기구 감쇠 단독값이나 모든 Sound1 시료의 확정 특성으로 해석하지 않는다.",
        "",
        "## 4. 원기록에서 확인할 값",
        "",
        "표 3. 보존한 확인 대상",
        "",
        "| 위치 | 원기록 | 이번 처리 |",
        "|---|---|---|",
        "| 3815Hz·75dB | 최대 954 / 평균 23 | 원값 보존. 평균 23만 주변 보간에서 제외하고 70dB 평균 503~80dB 평균 1,642를 연결했다. 최대 954는 정상적으로 사용했다. |",
        "| 100Hz·35~40dB | 평균 21→20 | 원값 보존. 이 구간의 보간 평균이 조금 내려갈 수 있으며 저음압 응답이 확정됐다는 뜻은 아니다. |",
        "| 100Hz·69~75dB | 평균 654→650→522→635(69/70/71/75dB) | 최대값은 증가하지만 평균은 요동한다. 임의로 정리하지 않고 그대로 반영했다. 읽기 위상·환경·기록을 재확인할 구간이다. |",
        "",
        "75dB·3815Hz의 평균을 70~80dB로 보간하면 약 909다. 이것은 원기록 확인을 위한 비교값일 뿐이며 원기록 23을 909로 고치지 않았다. 평균값 자체의 요동이 큰 구간에서는 빈칸을 채웠다는 사실만으로 예측의 정확도가 확보되지 않는다.",
        "",
        "## 5. 계산·코드 근거",
        "",
        "- [입력 원본](../../../tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/ADC%20테이블_입력원본.md): 수정 전 실측 51쌍과 1048Hz 기준 92개.",
        "- [재현 계산 스크립트](../../../tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/adc_estimate.py): 평균·최대별 비율 보간/외삽, 정수 반올림, 별도 50자리 Decimal 검산.",
        "- [CFX 빔포밍 믹서](../../../../src/1__cfx/systemControl/audioMixer.c): `tdc_audio_mix_2_buffers_for_beamforming`, 입력 스케일과 두 마이크 평균.",
        "- [블록 길이·입력 스케일](../../../../src/1__cfx/signalProcessing/definitionsForAlgorithm.h): 16표본, `AUDIO_INPUT_RSHIFT=5`.",
        "- [HEAR 최대값 계산](../../../../src/3__hear/microcode.hct): `HEAR_FC_agc_preprocessing`, 절대값 계산 뒤 최댓값 선택.",
        "- [CFX 공유값 기록](../../../../src/1__cfx/signalProcessing/agc.c): `audio_agc`, AGC 전 최대값 공유.",
        "",
        "기존 실측값의 보존과 산술 계산은 검사했으며, 빈칸에 대한 실제 음향 시험은 수행하지 않았다.",
        "",
    ]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", action="store_true")
    parser.add_argument("--patch", action="store_true")
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--document", action="store_true")
    args = parser.parse_args()
    table, reference, anchors, observations, filled, counts = build_data()
    document = render_document(reference, anchors, observations, filled, counts)
    if args.document:
        print(document)
        return
    if args.patch:
        if TARGET.read_bytes() != SOURCE.read_bytes():
            raise RuntimeError("대상 문서가 원본 보존 이후 변경됨. 사용자 수정부터 대조할 것")
        before = TARGET.read_text(encoding="utf-8").splitlines()
        lines = ["*** Begin Patch", "*** Update File: " + TARGET.as_posix(), "@@"]
        lines += ["-" + line for line in before]
        lines += ["+" + line for line in document.splitlines()]
        lines.append("*** End Patch")
        print("\n".join(lines))
        return
    if args.verify:
        actual = TARGET.read_text(encoding="utf-8")
        assert actual.rstrip() == document.rstrip(), "생성 문서와 적용 문서가 다름"
        actual_rows = parse_table(actual)
        assert actual_rows == annotated_rows(filled, observations, reference)
        for level, cells in table.items():
            assert actual_rows[level][3] == cells[3]
            for column, frequency in enumerate(FREQUENCIES):
                if frequency != 1048 and level in observations[frequency]:
                    assert actual_rows[level][column].split("<br>", 1)[0] == cells[column]
        print(json.dumps({"verified": True, **counts}, ensure_ascii=True))
        return
    report = {"counts": counts, "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(), "frequencies": {}}
    for frequency, data in observations.items():
        lo, hi = min(data), max(data)
        report["frequencies"][frequency] = {
            "range": [lo, hi],
            "first_ratios_max_mean": [x / reference[lo] for x in data[lo]],
            "last_ratios_max_mean": [x / reference[hi] for x in data[hi]],
            "median_55_80_ratios": [median(values[pos]/reference[level] for level, values in data.items() if 55 <= level <= 80 and (frequency, level, ("maximum", "mean")[pos]) not in EXCLUDED) for pos in (0, 1)],
            "120_cell": filled[120][FREQUENCIES.index(frequency)],
            "phase": phase_statistics(frequency),
        }
    report["phase_1048"] = phase_statistics(1048)
    report["3815_75_mean_comparison_only"] = interpolate(75, reference, anchors[3815, "mean"]).value
    report["examples"] = {level: filled[level] for level in (29, 56, 72, 74, 76, 80, 90, 110, 120)}
    print(json.dumps(report, ensure_ascii=True, indent=2))


if __name__ == "__main__":
    main()
