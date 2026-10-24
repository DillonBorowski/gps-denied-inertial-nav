import io
import unittest

from fit_gyro_bias import (
    GYRO_LSB_PER_DPS,
    fit_bias,
    load_capture,
    temp_counts_to_c,
    window_means,
)

CSV = """seq,t_us,ax,ay,az,gx,gy,gz
1,0,0,0,16384,10,-4,1
2,5000,0,0,16384,12,-2,3
3,10000,0,0,16384,14,0,5
4,15000,0,0,16384,16,2,7
"""

CSV_WITH_TEMP = """seq,t_us,ax,ay,az,gx,gy,gz,temp
1,0,0,0,16384,10,-4,1,21
2,5000,0,0,16384,12,-2,3,355
"""


class LoadCaptureTest(unittest.TestCase):
    def test_reads_gyro_columns_and_skips_header(self):
        t_us, gyro, temp = load_capture(io.StringIO(CSV))
        self.assertEqual(t_us, [0, 5000, 10000, 15000])
        self.assertEqual(gyro[0], (10, -4, 1))
        self.assertEqual(len(gyro), 4)

    def test_older_capture_without_temp_has_no_temp(self):
        _, _, temp = load_capture(io.StringIO(CSV))
        self.assertEqual(temp, [])

    def test_reads_temp_column_when_present(self):
        _, gyro, temp = load_capture(io.StringIO(CSV_WITH_TEMP))
        self.assertEqual(gyro[1], (12, -2, 3))
        self.assertEqual(temp, [21, 355])

    def test_skips_malformed_lines(self):
        text = CSV + "5,20000,1,2\n"
        _, gyro, _ = load_capture(io.StringIO(text))
        self.assertEqual(len(gyro), 4)


class TempConversionTest(unittest.TestCase):
    def test_room_temp_offset_reads_21c(self):
        self.assertAlmostEqual(temp_counts_to_c(21), 21.0)

    def test_sensitivity_is_333_87_counts_per_c(self):
        self.assertAlmostEqual(temp_counts_to_c(21 + 333.87), 22.0)


class FitBiasTest(unittest.TestCase):
    def test_bias_is_per_axis_mean_in_counts_and_dps(self):
        _, gyro, _ = load_capture(io.StringIO(CSV))
        fit = fit_bias(gyro)
        self.assertEqual(fit["samples"], 4)
        self.assertEqual(fit["bias_counts"], [13.0, -1.0, 4.0])
        self.assertAlmostEqual(fit["bias_dps"][0], 13.0 / GYRO_LSB_PER_DPS)

    def test_reports_per_axis_noise(self):
        _, gyro, _ = load_capture(io.StringIO(CSV))
        fit = fit_bias(gyro)
        # Sample stdev of 10,12,14,16 is sqrt(20/3).
        self.assertAlmostEqual(fit["stdev_counts"][0], (20 / 3) ** 0.5)

    def test_full_scale_is_250_dps(self):
        self.assertEqual(GYRO_LSB_PER_DPS, 131.0)


class WindowMeansTest(unittest.TestCase):
    def test_splits_into_equal_windows_and_drops_remainder(self):
        gyro = [(i, 0, 0) for i in range(10)]
        means = window_means(gyro, window_len=4)
        # 8 and 9 don't fill a window.
        self.assertEqual(means, [[1.5, 0.0, 0.0], [5.5, 0.0, 0.0]])

    def test_works_on_single_values_like_temp(self):
        self.assertEqual(window_means([(1,), (3,), (5,)], window_len=2),
                         [[2.0]])

    def test_no_windows_when_capture_is_shorter_than_one(self):
        self.assertEqual(window_means([(1, 2, 3)], window_len=4), [])


if __name__ == "__main__":
    unittest.main()
