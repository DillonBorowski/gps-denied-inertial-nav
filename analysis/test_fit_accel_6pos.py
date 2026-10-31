import io
import unittest

from fit_accel_6pos import (
    ACCEL_LSB_PER_G,
    check_pose,
    correct,
    fit_six_position,
    load_capture,
    pose_from_name,
    pose_mean,
)

CSV = """seq,t_us,ax,ay,az,gx,gy,gz,temp
1,0,10,-20,16000,20,60,2,300
2,5000,12,-22,16002,22,62,4,302
"""


def pose_reading(axis, sign, offset, scale):
    """Ideal mean reading for a pose, given per-axis offset and scale."""
    return tuple(offset[i] + (sign * scale[i] if i == axis else 0.0)
                 for i in range(3))


OFFSET = (120.0, -80.0, 300.0)
SCALE = (16500.0, 16300.0, 16100.0)
MEANS = {(axis, sign): pose_reading(axis, sign, OFFSET, SCALE)
         for axis in range(3) for sign in (1, -1)}


class LoadCaptureTest(unittest.TestCase):
    def test_reads_accel_gyro_and_temp(self):
        t_us, accel, gyro, temp = load_capture(io.StringIO(CSV))
        self.assertEqual(t_us, [0, 5000])
        self.assertEqual(accel[0], (10, -20, 16000))
        self.assertEqual(gyro[1], (22, 62, 4))
        self.assertEqual(temp, [300, 302])


class PoseFromNameTest(unittest.TestCase):
    def test_reads_axis_and_sign_from_file_name(self):
        self.assertEqual(pose_from_name("accel-6pos-xup-20261030.csv"), (0, 1))
        self.assertEqual(pose_from_name("accel-6pos-ydown-20261030.csv"),
                         (1, -1))
        self.assertEqual(pose_from_name("data/calibration/accel-6pos-zup.csv"),
                         (2, 1))

    def test_rejects_name_without_pose(self):
        with self.assertRaises(ValueError):
            pose_from_name("accel-20261030.csv")


class PoseMeanTest(unittest.TestCase):
    def test_trims_ends_and_drops_motion(self):
        # 10 s still at 200 Hz with a gyro spike in the middle whose accel
        # reading is junk. Trim 1 s each end; the spike is padded out.
        n = 2000
        accel = [(0, 0, 16000)] * n
        gyro = [(20, 60, 2)] * n
        accel[1000] = (5000, 5000, 5000)
        gyro[1000] = (2000, 60, 2)
        mean, used = pose_mean(accel, gyro, trim_s=1.0,
                               motion_threshold=100, motion_pad_s=0.5)
        self.assertEqual(mean, (0.0, 0.0, 16000.0))
        self.assertEqual(used, n - 400 - 201)

    def test_raises_when_nothing_left(self):
        with self.assertRaises(ValueError):
            pose_mean([(0, 0, 1)] * 10, [(0, 0, 0)] * 10, trim_s=1.0,
                      motion_threshold=100, motion_pad_s=0.5)


class CheckPoseTest(unittest.TestCase):
    def test_matching_pose_has_no_problems(self):
        self.assertEqual(check_pose((100, -50, 16000), 2, 1), [])

    def test_flags_wrong_axis(self):
        problems = check_pose((16000, 0, 100), 2, 1)
        self.assertEqual(len(problems), 1)
        self.assertIn("x", problems[0])

    def test_flags_wrong_sign(self):
        problems = check_pose((0, 0, -16000), 2, 1)
        self.assertEqual(len(problems), 1)
        self.assertIn("down", problems[0])


class FitSixPositionTest(unittest.TestCase):
    def test_recovers_offset_and_scale(self):
        offset, scale = fit_six_position(MEANS)
        for got, want in zip(offset, OFFSET):
            self.assertAlmostEqual(got, want)
        for got, want in zip(scale, SCALE):
            self.assertAlmostEqual(got, want)

    def test_needs_all_six_poses(self):
        partial = dict(MEANS)
        del partial[(1, -1)]
        with self.assertRaises(ValueError):
            fit_six_position(partial)

    def test_corrected_poses_read_one_g(self):
        offset, scale = fit_six_position(MEANS)
        for (axis, sign), m in MEANS.items():
            g = correct(m, offset, scale)
            self.assertAlmostEqual(g[axis], float(sign))
            for i in range(3):
                if i != axis:
                    self.assertAlmostEqual(g[i], 0.0)


class ConstantsTest(unittest.TestCase):
    def test_full_scale_is_2g(self):
        self.assertEqual(ACCEL_LSB_PER_G, 16384.0)


if __name__ == "__main__":
    unittest.main()
