"""추가 ADC 평균값을 기존 1048Hz 기준곡선에 비율 보간한다.

입력 원본은 그대로 보존한다. 문서/패치는 표준 출력으로만 제공하며
실제 문서 적용은 별도로 수행한다. 평균으로부터 최대값을 만들지 않는다.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re

import adc_estimate as base


TASK = Path(__file__).resolve().parent
SOURCE = TASK / "ADC 추가 테이블_입력원본.md"
TARGET = base.ROOT / "docs/참고/인허가/사운드1/ADC 추가 테이블.md"
FREQUENCIES = (1438, 1500, 1593, 2869, 3217)
MEASURED_LEVELS = (65, 70, 75, 80)


def parse_observations(text: str) -> dict[int, dict[int, int]]:
    """원표의 열 순서·측정 행·양수를 검사하고 주파수별 평균값을 읽는다."""
    headers = re.findall(r"(\d+)\s*Hz", text)
    if tuple(map(int, headers)) != FREQUENCIES:
        raise ValueError("추가 측정 주파수의 순서 또는 개수가 바뀜")
    result = {frequency: {} for frequency in FREQUENCIES}
    seen = set()
    for line in text.splitlines():
        if not re.match(r"^\|\s*\d+\s*\|", line):
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if cells[-1] == "":
            cells.pop()
        if len(cells) != 6 or not all(re.fullmatch(r"\d+", cell) for cell in cells):
            raise ValueError(f"추가 실측 행 형식 오류: {line}")
        level, *values = map(int, cells)
        if level not in MEASURED_LEVELS or level in seen:
            raise ValueError(f"예상 밖 또는 중복 음압: {level}")
        if any(not 0 < value <= base.DISPLAY_LIMIT for value in values):
            raise ValueError(f"평균 ADC 범위 오류: {line}")
        seen.add(level)
        for frequency, value in zip(FREQUENCIES, values):
            result[frequency][level] = value
    if seen != set(MEASURED_LEVELS):
        raise ValueError("65·70·75·80dB 측정 행이 모두 필요함")
    return result


def build_data():
    observations = parse_observations(SOURCE.read_text(encoding="utf-8"))
    _, reference, _, _ = base.prepare()
    rows = {}
    limited = []
    counts = {"measured": 0, "interpolated": 0, "extrapolated": 0, "limited": 0}
    for level in range(29, 121):
        row = []
        for frequency, points in observations.items():
            if level in points:
                value = points[level]
                counts["measured"] += 1
            else:
                estimate = base.interpolate(level, reference, points)
                value = estimate.value
                counts["extrapolated" if estimate.extrapolated else "interpolated"] += 1
                if estimate.limited:
                    limited.append((level, frequency, estimate.raw))
                    counts["limited"] += 1
            independent = base.independent_value(level, reference, points)
            if value != independent:
                raise AssertionError((level, frequency, value, independent))
            row.append(value)
        rows[level] = row
    return reference, observations, rows, limited, counts


def numeric_rows(text: str) -> dict[int, list[int]]:
    """표2의 92행을 읽는다. 작은 시작·끝 주석은 숫자로 섞지 않는다."""
    result = {}
    for line in text.splitlines():
        if not re.match(r"^\|\s*\d+\s*\|", line):
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) != 7:
            continue
        level = int(cells[0])
        values = [cell.split("<br>", 1)[0].replace("*", "").replace(",", "") for cell in cells[1:]]
        if level in result or not all(re.fullmatch(r"\d+", value) for value in values):
            raise ValueError(f"확장 표 행 형식 오류: {line}")
        result[level] = list(map(int, values))
    if sorted(result) != list(range(29, 121)):
        raise ValueError("확장 표는 29~120dB 92행이어야 함")
    return result


def render_document() -> str:
    reference, observations, rows, limited, counts = build_data()
    lines = [
        "# Sound1 추가 주파수별 ADC 테이블",
        "",
        "1438·1500·1593·2869·3217Hz의 실측값20개를 보존하고, 기존1048Hz 기준곡선과 같은 방식으로 **29~120dB SPL의 평균 ADC 표**를 구성했다. **추가한440개 값은 계산값이며 새로 측정한 결과가 아니다.**",
        "",
        "원표에 괄호가 없으므로 앞서 정한 표기 원칙에 따라 모두 평균값으로 해석했다. 여기서 평균은 무선으로 읽은 ADC 공유값들의 평균이며, 오디오 원표본의 평균이나 RMS가 아니다. 이번 자료에는 최대값이 없으므로 최대값 열은 만들지 않았다.",
        "",
        "## 1. 제공된 측정값",
        "",
        "표 1. 추가 주파수의 실측 평균 ADC",
        "",
        "| 음압 dB SPL | 1438 Hz | 1500 Hz | 1593 Hz | 2869 Hz | 3217 Hz |",
        "|---:|---:|---:|---:|---:|---:|",
    ]
    for level in MEASURED_LEVELS:
        lines.append("| " + " | ".join([str(level), *(f"{observations[f][level]:,}" for f in FREQUENCIES)]) + " |")
    lines += [
        "",
        "주파수는 원기록의1438Hz·1593Hz를 그대로 사용했다. FFT bin의 정확한 중심주파수인1437.5Hz·1593.75Hz로 바꾼 것이 아니다.",
        "",
        "## 2. 29~120dB SPL 확장 표",
        "",
        "- **굵은 숫자20개는 실측 평균**, 나머지5주파수 열의 숫자는 계산값이다. 1048Hz 열은 기존 기준곡선의 중심값이며 이번에 동시에 측정한 평균값이 아니다.",
        "- 실측은65·70·75·80dB에만 있다. 그 사이 빈칸60개는 보간,29~64dB와81~120dB의380개는 외삽으로 채웠다. 숫자 앞에 별도 계산 기호는 붙이지 않았다.",
        "- 65dB의 시작·80dB의 끝에만 작은 글씨를 붙였다. `1048Hz 66.0dB`는 해당 **평균 ADC**와 같은 숫자가1048Hz 기준곡선에서 약66.0dB에 해당한다는 뜻이다. 입력 음압을 고치거나 유효 성능 범위를 승인한 표시가 아니다.",
        "- 상한을 넘는7칸은 앞선 표처럼262,143으로 표시했다. 해당 위치와 제한 전 값은 표4에 수록했다. **고음압 외삽이나 표시상한 도달을 실제 측정 가능 범위·합격 기준으로 해석하지 않는다.**",
        "",
        "표 2. 추가 주파수의 평균 ADC 확장값과1048Hz 기준",
        "",
        "| 음압 dB SPL | 1048 Hz 기준 | 1438 Hz | 1500 Hz | 1593 Hz | 2869 Hz | 3217 Hz |",
        "|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for level, values in rows.items():
        cells = [str(level), f"{reference[level]:,}"]
        for frequency, value in zip(FREQUENCIES, values):
            cell = f"{value:,}"
            if level in observations[frequency]:
                cell = f"**{cell}**"
            if level in (65, 80):
                label = "시작" if level == 65 else "끝"
                equivalent = base.equivalent_1048_level(value, reference)
                cell += f"<br><small>{label} · 1048Hz {equivalent:.1f}dB</small>"
            cells.append(cell)
        lines.append("| " + " | ".join(cells) + " |")
    lines += [
        "",
        "## 3. 계산 방법과 적용 범위",
        "",
        "### 3.1. 측정한 차이를1048Hz 기준곡선에 반영",
        "",
        "음압을 L, 같은 행의1048Hz 기준값을 R(L), 추가 주파수의 실측 평균을 A(L)라 하면 비율은 `k(L)=A(L)/R(L)`이다. 이 비율은 이번 관측값과 기준곡선의 차이이며, 기구나 빔포밍의 감쇠만 분리해 측정한 값은 아니다.",
        "",
        "두 실측 음압 L₁·L₂ 사이에서는 다음 식으로 비율을 연결한다. 예전 표와 마찬가지로 단순 ADC 차이가 아니라 로그 비율을 보간하므로,1048Hz 기준곡선의 형태와 실측 차이가 함께 반영된다.",
        "",
        "`t=(L−L₁)/(L₂−L₁)`",
        "",
        "`평균 ADC 추정값=R(L)×[A(L₁)/R(L₁)]^(1−t)×[A(L₂)/R(L₂)]^t`",
        "",
        f"예를 들어1500Hz·66dB는 R(66)={reference[66]:,}, R(65)={reference[65]:,}, R(70)={reference[70]:,}, 실측 평균501·882를 사용한다. `503×(501/452)^0.8×(882/776)^0.2`를 정수로 반올림하면 **{rows[66][1]:,}**이다.",
        "",
        "### 3.2. 측정 범위 밖은 가까운 끝점 비율 유지",
        "",
        "65dB보다 낮으면65dB의 비율,80dB보다 높으면80dB의 비율을 그대로 유지한다. 이는 계산을 연장하는 가정이며, 저음압 잡음 바닥이나 고음압 압축·왜곡을 실측한 것은 아니다. 실제 주파수별 끝단 특성이 기존1048Hz 곡선과 다르면 외삽값도 달라진다.",
        "",
        "표 3. 외삽에 사용한 실측점과1048Hz 대비 비율",
        "",
        "| 주파수 | 65dB 실측 평균 | 아래쪽 비율 | 80dB 실측 평균 | 위쪽 비율 |",
        "|---:|---:|---:|---:|---:|",
    ]
    for frequency, points in observations.items():
        lines.append(f"| {frequency} Hz | {points[65]:,} | {points[65]/reference[65]:.6f} | {points[80]:,} | {points[80]/reference[80]:.6f} |")
    lines += [
        "",
        f"표3의 비율은 소수6자리로 표시했지만 계산에는 실측 평균/1048Hz 기준이라는 원래 비율을 사용했다. 예를 들어1438Hz·110dB는 `81,809×2,789/2,487`을 반올림한 **{rows[110][0]:,}**이다.",
        "",
        "### 3.3. 상한으로 제한한 위치",
        "",
        "262,143은 앞선 ADC 표와 맞춘 표시상한이다. 상한 이전의 압축·왜곡이나 실제 포화 평균은 이 계산에 포함되지 않는다. 따라서 반복되는262,143은 제품이 해당 음압에서 그 평균값을 출력한다는 예측이 아니다.",
        "",
        "표 4. 표시상한으로 제한한7개 계산값",
        "",
        "| 음압 dB SPL | 주파수 | 제한 전 계산값 | 표2 표시값 |",
        "|---:|---:|---:|---:|",
    ]
    for level, frequency, raw in limited:
        lines.append(f"| {level} | {frequency} Hz | {math.floor(raw + 0.5):,} | {base.DISPLAY_LIMIT:,} |")
    lines += [
        "",
        "### 3.4. 시작·끝의1048Hz 대응 음압",
        "",
        "추가 주파수의 평균 ADC를 사이에 두는1048Hz 기준곡선의 두 행을 찾아 로그 ADC 축에서 역산했다. R(L₁)≤A≤R(L₂)이면 다음 식을 사용한다.",
        "",
        "`대응 음압=L₁+(L₂−L₁)×ln[A/R(L₁)]/ln[R(L₂)/R(L₁)]`",
        "",
        "예를 들어1438Hz·65dB의 실측 평균503은1048Hz 기준곡선의66dB 값503과 같다. 따라서 해당 칸에 `시작 · 1048Hz 66.0dB`로 표시했다. 이는 같은 ADC 숫자끼리의 비교이며 실제 음압 환산·측정기 교정을 뜻하지 않는다.",
        "",
        "### 3.5. 기존 표와 함께 사용할 조건",
        "",
        "이번 추가 측정도 앞선 표와 같은 APx 표시·보정, 조립 상태, 왼쪽 빔포밍·정면 배치에서 얻었다는 전제로 기존1048Hz 곡선을 연결했다. 원기록의 음압에 과거 보정값을 다시 더하거나 빼지 않았고, 빔포밍·기구 감쇠도 별도로 한 번 더 적용하지 않았다. 조건이 달랐다면 두 측정군은 분리해야 한다.",
        "",
        "이 표는 평균 관측값의 확장 자료다. 기존 최대값 기반 시험 기준안을 평균 기준으로 바꾸거나, 이번 계산값 자체로 정식 합격 범위를 확정한 것은 아니다. 특히 실측이 없는 저음압·고음압 구간은 추가 측정 전 참고값으로 사용한다.",
        "",
        "## 4. 근거와 검증",
        "",
        "- [기존 주파수별 ADC 표](ADC%20테이블.md):1048Hz 기준92개와 비율 보간·외삽 방식.",
        "- [추가 측정 원본](../../../tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/ADC%20추가%20테이블_입력원본.md):사용자가 입력한5주파수·4음압의20개 값.",
        "- [재현 계산 스크립트](../../../tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/adc_extra_estimate.py):평균값 보간·외삽·표시상한·시작과 끝의 대응값 계산.",
        "- [수치·원값·문서 검사](../../../tasks/인허가/20260923_1206_ADC-주파수별-빈칸보완/test_adc_extra_estimate.py):독립Decimal 검산과 원측정20개·1048Hz 기준92개 보존 확인.",
        "",
        f"새로 채운 값은{counts['interpolated'] + counts['extrapolated']}개이며 실제 음향 시험 결과가 아니다. 실측값 보존과 산술 계산의 검증은 제품의 끝단 응답이나 생산품 대표성 검증을 대신하지 않는다.",
        "",
    ]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--document", action="store_true")
    mode.add_argument("--patch", action="store_true")
    mode.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    document = render_document()
    if args.document:
        print(document, end="")
        return
    if args.patch:
        actual = TARGET.read_text(encoding="utf-8")
        if actual.rstrip() not in (SOURCE.read_text(encoding="utf-8").rstrip(), document.rstrip()):
            raise RuntimeError("대상 문서가 원본 또는 생성본과 다름. 사용자 변경부터 대조할 것")
        patch = ["*** Begin Patch", "*** Update File: " + TARGET.as_posix(), "@@"]
        patch += ["-" + line for line in actual.splitlines()]
        patch += ["+" + line for line in document.splitlines()]
        patch.append("*** End Patch")
        print("\n".join(patch))
        return
    reference, observations, rows, limited, counts = build_data()
    if args.verify:
        actual = TARGET.read_text(encoding="utf-8")
        if actual.rstrip() != document.rstrip():
            raise AssertionError("추가 표 문서와 생성 결과가 다름")
        if numeric_rows(actual) != {level: [reference[level], *values] for level, values in rows.items()}:
            raise AssertionError("표2 값이 생성 결과와 다름")
    report = {
        "verified": args.verify,
        "counts": counts,
        "input_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
        "reference_sha256": hashlib.sha256(base.SOURCE.read_bytes()).hexdigest(),
        "limited_cells": [(level, frequency, math.floor(raw + 0.5)) for level, frequency, raw in limited],
        "equivalent_endpoints": {f: [base.equivalent_1048_level(p[l], reference) for l in (65, 80)] for f, p in observations.items()},
        "examples": {level: rows[level] for level in (29, 65, 66, 70, 80, 110, 120)},
    }
    print(json.dumps(report, ensure_ascii=True, indent=2))


if __name__ == "__main__":
    main()
