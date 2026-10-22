import unittest

from capture_imu import parse_sample_line, summarize


class ParseSampleLineTest(unittest.TestCase):
    def test_parses_full_line(self):
        self.assertEqual(
            parse_sample_line("7,123456,1,-2,3,4,5,-6\n"),
            (7, 123456, [1, -2, 3, 4, 5, -6]),
        )

    def test_header_and_comment_lines_are_not_samples(self):
        self.assertIsNone(parse_sample_line("seq,t_us,ax,ay,az,gx,gy,gz\n"))
        self.assertIsNone(parse_sample_line("# WHO_AM_I = 0xEA\n"))

    def test_partial_line_is_not_a_sample(self):
        # Opening the port mid-stream can hand us the tail of a line.
        self.assertIsNone(parse_sample_line("3,4,5\n"))
        self.assertIsNone(parse_sample_line(""))


class SummarizeTest(unittest.TestCase):
    def test_counts_no_gaps_for_consecutive_seq(self):
        s = summarize([1, 2, 3, 4], [0, 5000, 10000, 15000])
        self.assertEqual(s["samples"], 4)
        self.assertEqual(s["missed"], 0)

    def test_counts_missed_samples_from_seq_gaps(self):
        # 3 and 4 are missing, then 7 is missing.
        s = summarize([1, 2, 5, 6, 8], [0, 5000, 20000, 25000, 35000])
        self.assertEqual(s["missed"], 3)

    def test_interval_stats_only_use_consecutive_samples(self):
        # The 1 -> 3 step spans a dropped sample, so its 10 ms interval must
        # not count as jitter.
        s = summarize([1, 2, 3, 5], [0, 5000, 10100, 20000])
        self.assertEqual(s["dt_min_us"], 5000)
        self.assertEqual(s["dt_max_us"], 5100)
        self.assertAlmostEqual(s["dt_mean_us"], 5050)

    def test_seq_wraparound_is_not_a_gap(self):
        s = summarize([2**32 - 1, 0], [0, 5000])
        self.assertEqual(s["missed"], 0)
        self.assertEqual(s["dt_max_us"], 5000)


if __name__ == "__main__":
    unittest.main()
