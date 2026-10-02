"""수치 모델 검사. 장비 실행, 실제 CFX 바이너리 실행과 구분한다."""
from decimal import Decimal, ROUND_HALF_UP, localcontext
import hashlib
import math
import unittest

import adc_frequency_model as model
from adc_estimate import SOURCE, TARGET, parse_table, phase_statistics, equivalent_1048_level


class FrequencyModelTests(unittest.TestCase):
    def test_input_and_original_cells_preserved(self):
        self.assertEqual(hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                         "bd3ca62e05c4894227815c71fe221652be1695bb0d1c656c0b784426aaf07112")
        original = parse_table(SOURCE.read_text(encoding="utf-8"))
        current = parse_table(TARGET.read_text(encoding="utf-8"))
        count = 0
        for level, cells in original.items():
            for i, value in enumerate(cells):
                if value.replace("*", "").replace("_", "") != "-":
                    self.assertEqual(current[level][i].split("<br>", 1)[0], value)
                    count += 1
        self.assertEqual(count, 143)

    def test_equivalent_level_and_endpoint_annotations(self):
        reference = model.REFERENCE
        for level, value in reference.items():
            self.assertEqual(equivalent_1048_level(value, reference), level)
        for invalid in (0, 19, 262144, float("nan"), float("inf")):
            with self.assertRaises(ValueError):
                equivalent_1048_level(invalid, reference)
        actual = parse_table(TARGET.read_text(encoding="utf-8"))
        annotation_count = 0
        for f, data in model.OBSERVATIONS.items():
            column = model.FREQUENCIES.index(f)
            for level, cells in actual.items():
                if level not in (min(data), max(data)):
                    self.assertNotIn("<small>", cells[column])
                    continue
                annotation_count += 1
                value = data[level][0]
                # 독립 검산: Decimal log10 역산으로 출력 소수 한 자리까지 비교.
                with localcontext() as context:
                    context.prec = 45
                    levels = sorted(reference)
                    if value in reference.values():
                        expected = Decimal(next(x for x in levels if reference[x] == value))
                    else:
                        lo = max(x for x in levels if reference[x] < value)
                        hi = min(x for x in levels if reference[x] > value)
                        weight = (Decimal(value)/reference[lo]).log10() / (Decimal(reference[hi])/reference[lo]).log10()
                        expected = Decimal(lo) + (hi-lo)*weight
                    expected = expected.quantize(Decimal("0.1"), rounding=ROUND_HALF_UP)
                self.assertIn(f"1048Hz {expected:.1f}dB</small>", cells[column])
        self.assertEqual(annotation_count, 14)
        self.assertEqual(sum("<small>" in cell for cells in actual.values() for cell in cells), 14)
        self.assertFalse(any(cell.startswith(("외", "≈", "상")) for cells in actual.values() for cell in cells))

    def test_beam_gain_independent_sine_sum(self):
        # 출력을 사인/코사인으로 직접 투영한다. 코사인 반각 식을 사용하지 않는다.
        for f in (100, 128, 1048, 2869, 3815, 4037, 7900, 7968.75):
            for angle in (0, 15, 30, 90, 180):
                delta = model.DELAY_SAMPLES/model.FS - .022*math.cos(math.radians(angle))/343
                phase_difference = 2*math.pi*f*delta
                sine_coefficient = cosine_coefficient = 0.0
                for i in range(512):
                    phase = 2*math.pi*i/512
                    signal = (math.sin(phase) + math.sin(phase-phase_difference))/2
                    sine_coefficient += signal*math.sin(phase)*2/512
                    cosine_coefficient += signal*math.cos(phase)*2/512
                self.assertAlmostEqual(math.hypot(sine_coefficient, cosine_coefficient),
                                       model.beam_gain(f,angle), places=12)

    def test_phase_statistics_independent_samples(self):
        # 균등 시작 위상에서 16표본 sin()을 직접 계산. 위상 간격 공식을 쓰지 않는다.
        count = 32768
        tested = (*model.FREQUENCIES, 250, 500, 2869, 3217, 4000, 7968.75, 8000)
        for f in tested:
            offsets = [2*math.pi*f*n/model.FS for n in range(16)]
            total, minimum = 0.0, 1.0
            for i in range(count):
                phase = math.pi*(i+.5)/count
                peak = max(abs(math.sin(phase+offset)) for offset in offsets)
                total += peak
                minimum = min(minimum, peak)
            analytic = phase_statistics(f)
            self.assertAlmostEqual(total/count, analytic["uniform_mean"], delta=1e-8)
            self.assertAlmostEqual(minimum, analytic["worst"], delta=math.pi/count)

    def test_coherent_sampling_does_not_recover_peak(self):
        for f, phase, expected in ((4000, math.pi/4, math.sqrt(.5)), (8000, 0, 0)):
            maximum = max(abs(math.sin(2*math.pi*f*n/model.FS+phase)) for n in range(16000))
            self.assertAlmostEqual(maximum, expected, delta=1e-10)
        # 100ms 폴링은 100/7900Hz에서 같은 16표본 패턴을 반복할 수 있다.
        for f in (100, 7900):
            values = [max(abs(math.sin(2*math.pi*f*(1600*k+n)/model.FS+.13))
                          for n in range(16)) for k in range(30)]
            self.assertLess(max(values)-min(values), 1e-9)

    def test_contiguous_100ms_bound(self):
        for f in model.FREQUENCIES:
            bound = model.continuous_window_worst(f)
            self.assertGreater(model.db(bound), -.002)
            # 연속 표본을 직접 평가해 얻은 값은 해석적 최악값보다 낮아지지 않는다.
            for i in range(128):
                phase = math.pi*(i+.37)/128
                peak = max(abs(math.sin(2*math.pi*f*n/model.FS+phase)) for n in range(1600))
                self.assertGreaterEqual(peak+1e-10, bound)
        self.assertAlmostEqual(model.continuous_window_worst(4000), math.sqrt(.5), delta=1e-10)
        self.assertLess(model.continuous_window_worst(8000), 1e-10)

    def test_quantization_bound_signed_integer(self):
        # CFX 산술 우측 시프트와 동일한 정수 연산이라는 조건부 검사.
        values = [*range(-128,129), -(1<<23), -(1<<23)+1, (1<<23)-2, (1<<23)-1]
        for x in values:
            for y in values:
                calculated = ((x>>5)+(y>>5))>>1
                difference = (x+y)/64 - calculated
                self.assertGreaterEqual(difference, 0)
                self.assertLess(difference, 2)

    def test_independent_decimal_centers_and_monotonicity(self):
        with localcontext() as ctx:
            ctx.prec = 45
            for f in model.FREQUENCIES:
                factor = Decimal(10) ** (Decimal(str(model.COEFFICIENTS[f]["db"]))/20)
                sequence = []
                for level, reference in model.REFERENCE.items():
                    expected = int((Decimal(reference)*factor).quantize(Decimal(1), rounding=ROUND_HALF_UP))
                    self.assertEqual(model.center(f,level), expected)
                    sequence.append(expected)
                # 정수 ADC 반올림으로 이웃 두 저음압 행이 같을 수 있으나 감소하지 않는다.
                self.assertTrue(all(a <= b for a,b in zip(sequence,sequence[1:])))
        self.assertEqual(model.center(1048,120), 262143)

    def test_limits_and_saturation_not_hidden(self):
        self.assertEqual(model.limits(1048,70), (452,1389))
        self.assertEqual(model.limits(1048,29)[0], None)
        self.assertEqual(model.limits(1048,120)[1], None)
        self.assertEqual(model.limits(1048,34)[0], 20)
        self.assertGreater(model.center(3038,119), 262143)
        self.assertEqual(model.summary()["over_limit_centers"], [(3038,119,287055),(3038,120,322506)])

    def test_fit_and_nonfit_comparisons_separate(self):
        rows = model.compare_measurements()
        self.assertEqual(len(rows), 51)
        self.assertEqual(sum(row["fit"] for row in rows), 42)
        self.assertEqual(sum(not row["fit"] for row in rows), 9)
        self.assertTrue(all(row["inside"] for row in rows))

    def test_interpolated_frequency_boundaries(self):
        for f in model.FREQUENCIES:
            self.assertEqual(model.empirical_prediction(f,70)["adc"], model.center(f,70))
        self.assertIsNone(model.empirical_prediction(99,70))
        self.assertIsNone(model.empirical_prediction(7968.75,70))
        self.assertEqual(model.empirical_prediction(2869,70)["bracket_hz"], [1048,3038])
        self.assertFalse(model.empirical_prediction(2869,70)["measured_frequency"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
