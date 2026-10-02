"""실측 원값·기준곡선·독립 산술·문서 구조를 검사한다. 음향 시험은 아니다."""
from decimal import Decimal, ROUND_HALF_UP, localcontext
import hashlib
import re
import unittest

import adc_extra_estimate as extra


class ExtraAdcTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.reference, cls.points, cls.rows, cls.limited, cls.counts = extra.build_data()

    def test_source_measurements_and_reference_preserved(self):
        expected = {
            1438: {65: 503, 70: 894, 75: 1572, 80: 2789},
            1500: {65: 501, 70: 882, 75: 1563, 80: 2769},
            1593: {65: 509, 70: 891, 75: 1580, 80: 2833},
            2869: {65: 416, 70: 725, 75: 1285, 80: 2277},
            3217: {65: 588, 70: 1053, 75: 1858, 80: 3280},
        }
        self.assertEqual(self.points, expected)
        for column, frequency in enumerate(extra.FREQUENCIES):
            for level, value in expected[frequency].items():
                self.assertEqual(self.rows[level][column], value)
        self.assertEqual(
            hashlib.sha256(extra.base.SOURCE.read_bytes()).hexdigest(),
            "bd3ca62e05c4894227815c71fe221652be1695bb0d1c656c0b784426aaf07112",
        )
        current_reference = extra.base.parse_table(extra.base.TARGET.read_text(encoding="utf-8"))
        self.assertEqual(self.reference, {l: int(c[3].replace(",", "")) for l, c in current_reference.items()})

    def test_all_values_against_independent_decimal_formula(self):
        with localcontext() as context:
            context.prec = 50
            for column, frequency in enumerate(extra.FREQUENCIES):
                measured = self.points[frequency]
                for level, values in self.rows.items():
                    if level <= 65:
                        raw = Decimal(self.reference[level]) * measured[65] / self.reference[65]
                    elif level >= 80:
                        raw = Decimal(self.reference[level]) * measured[80] / self.reference[80]
                    else:
                        lower = 65 + ((level - 65) // 5) * 5
                        upper = lower + 5
                        weight = Decimal(level - lower) / 5
                        # 기존 ln/exp 구현과 별도로 dB 축에서 계산한다.
                        low_db = 20 * (Decimal(measured[lower]).log10() - Decimal(self.reference[lower]).log10())
                        high_db = 20 * (Decimal(measured[upper]).log10() - Decimal(self.reference[upper]).log10())
                        adc_db = 20 * Decimal(self.reference[level]).log10() + low_db + weight * (high_db - low_db)
                        raw = Decimal(10) ** (adc_db / 20)
                    expected = min(262143, int(raw.quantize(Decimal(1), rounding=ROUND_HALF_UP)))
                    self.assertEqual(values[column], expected, (frequency, level))

    def test_counts_and_clipped_positions(self):
        self.assertEqual(self.counts, {"measured": 20, "interpolated": 60, "extrapolated": 380, "limited": 7})
        self.assertEqual(
            {(l, f) for l, f, _ in self.limited},
            {(120, 1438), (120, 1500), (119, 1593), (120, 1593), (118, 3217), (119, 3217), (120, 3217)},
        )
        for level, frequency, raw in self.limited:
            self.assertGreater(raw, 262143)
            self.assertEqual(self.rows[level][extra.FREQUENCIES.index(frequency)], 262143)
        self.assertEqual(self.rows[120][3], 240008)

    def test_complete_positive_monotonic_rows(self):
        self.assertEqual(sorted(self.rows), list(range(29, 121)))
        for column in range(5):
            values = [self.rows[level][column] for level in range(29, 121)]
            self.assertTrue(all(0 < value <= 262143 for value in values))
            self.assertTrue(all(a <= b for a, b in zip(values, values[1:])))

    def test_equivalent_endpoints_independently(self):
        document = extra.render_document()
        self.assertEqual(document.count("<small>시작"), 5)
        self.assertEqual(document.count("<small>끝"), 5)
        with localcontext() as context:
            context.prec = 45
            for measured in self.points.values():
                for level in (65, 80):
                    value = measured[level]
                    if value in self.reference.values():
                        expected = Decimal(next(l for l, a in self.reference.items() if a == value))
                    else:
                        lower = max(l for l, a in self.reference.items() if a < value)
                        lo, hi = Decimal(self.reference[lower]), Decimal(self.reference[lower + 1])
                        expected = Decimal(lower) + (Decimal(value).log10() - lo.log10()) / (hi.log10() - lo.log10())
                    actual = extra.base.equivalent_1048_level(value, self.reference)
                    self.assertAlmostEqual(actual, float(expected), places=11)
                    self.assertIn(f"1048Hz {float(expected):.1f}dB</small>", document)
        for line in document.splitlines():
            if "<small>" in line:
                self.assertRegex(line, r"^\| (65|80) \|")

    def test_document_contract_and_applied_content(self):
        expected = extra.render_document()
        actual = extra.TARGET.read_text(encoding="utf-8")
        self.assertEqual(actual.rstrip(), expected.rstrip())
        parsed = extra.numeric_rows(actual)
        self.assertEqual(parsed, {level: [self.reference[level], *row] for level, row in self.rows.items()})
        extended = actual.split("표 2.", 1)[1].split("## 3.", 1)[0]
        self.assertEqual(len(re.findall(r"\*\*[\d,]+\*\*", extended)), 20)
        self.assertNotIn("(최대값)", extended)
        self.assertNotIn("≈", extended)
        self.assertIn("이번 자료에는 최대값이 없으므로", actual)
        self.assertIn("추가한440개 값은 계산값", actual)

    def test_invalid_source_rejected(self):
        source = extra.SOURCE.read_text(encoding="utf-8")
        row65 = next(line for line in source.splitlines() if re.match(r"^\|\s*65\s*\|", line))
        cases = [
            source.replace("1438 Hz", "1437.5 Hz"),
            source.replace(row65, ""),
            source + "\n" + row65,
            source.replace("503", "0", 1),
            source.replace("503", "-503", 1),
            source.replace("503", "(503) 500", 1),
        ]
        for text in cases:
            with self.subTest(text=text[:80]), self.assertRaises(ValueError):
                extra.parse_observations(text)


if __name__ == "__main__":
    unittest.main()
