import unittest

from capture_imu import INFO_END, collect_info, parse_sample_line, summarize


class ParseSampleLineTest(unittest.TestCase):
    def test_parses_full_line(self):
        self.assertEqual(
            parse_sample_line("7,123456,1,-2,3,4,5,-6\n"),
            (7, 123456, [1, -2, 3, 4, 5, -6]),
        )

    def test_parses_line_with_temp_column(self):
        self.assertEqual(
            parse_sample_line("7,123456,1,-2,3,4,5,-6,1500\n"),
            (7, 123456, [1, -2, 3, 4, 5, -6, 1500]),
        )

    def test_header_and_comment_lines_are_not_samples(self):
        self.assertIsNone(parse_sample_line("seq,t_us,ax,ay,az,gx,gy,gz\n"))
        self.assertIsNone(
            parse_sample_line("seq,t_us,ax,ay,az,gx,gy,gz,temp\n"))
        self.assertIsNone(parse_sample_line("# WHO_AM_I = 0xEA\n"))

    def test_partial_line_is_not_a_sample(self):
        # Opening mid-stream can give a partial line.
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
        # The 3 -> 5 gap shouldn't count as jitter.
        s = summarize([1, 2, 3, 5], [0, 5000, 10100, 20000])
        self.assertEqual(s["dt_min_us"], 5000)
        self.assertEqual(s["dt_max_us"], 5100)
        self.assertAlmostEqual(s["dt_mean_us"], 5050)

    def test_seq_wraparound_is_not_a_gap(self):
        s = summarize([2**32 - 1, 0], [0, 5000])
        self.assertEqual(s["missed"], 0)
        self.assertEqual(s["dt_max_us"], 5000)


def fake_readline(lines):
    it = iter(lines)
    return lambda: next(it, "")


class CollectInfoTest(unittest.TestCase):
    def test_keeps_comment_lines_up_to_end_marker(self):
        lines = ["1,0,0,0,0,0,0,0,0\n", "# gyro bias: 1 2 3\n",
                 "2,5000,0,0,0,0,0,0,0\n", "# accel offset: 4 5 6\n",
                 INFO_END + "\n", "# later message\n"]
        info = collect_info(fake_readline(lines), max_lines=100)
        self.assertEqual(info, ["# gyro bias: 1 2 3", "# accel offset: 4 5 6"])

    def test_returns_none_when_marker_never_comes(self):
        # Old firmware ignores the request; readline times out with "".
        lines = ["# WHO_AM_I = 0xEA\n", "1,0,0,0,0,0,0,0,0\n"]
        self.assertIsNone(collect_info(fake_readline(lines), max_lines=100))

    def test_gives_up_after_max_lines(self):
        lines = ["1,0,0,0,0,0,0,0,0\n"] * 50 + [INFO_END + "\n"]
        self.assertIsNone(collect_info(fake_readline(lines), max_lines=10))


if __name__ == "__main__":
    unittest.main()
