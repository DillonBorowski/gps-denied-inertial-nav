"""Fit accelerometer offset and scale from six stationary poses.

Usage:
    python analysis/fit_accel_6pos.py data/calibration/accel-6pos-*-20261030.csv
    python analysis/fit_accel_6pos.py FILES... --out data/calibration/accel-6pos.json

One capture per pose, each axis pointing straight up and straight down. The
pose comes from the file name (xup, xdown, yup, ydown, zup, zdown). Bumps are
found from the gyro and cut out, as in fit_gyro_bias.py.

Model per axis: g = (raw - offset) / scale, scale in counts per g.
Six poses can't see cross-axis misalignment; the corrected |g| per pose
shows how much that (plus tilt) matters.
"""

import argparse
import json
import math
import pathlib
import re
import statistics

from capture_imu import parse_sample_line
from fit_gyro_bias import SAMPLE_RATE_HZ, motion_mask, temp_counts_to_c

ACCEL_LSB_PER_G = 16384.0  # +/-2 g
AXES = "xyz"
POSE_RE = re.compile(r"([xyz])(up|down)")


def pose_name(axis, sign):
    return AXES[axis] + ("up" if sign > 0 else "down")


def load_capture(lines):
    """(t_us, accel xyz, gyro xyz, temp counts). temp empty for old captures."""
    t_us, accel, gyro, temp = [], [], [], []
    for line in lines:
        sample = parse_sample_line(line)
        if sample is None:
            continue
        _, t, values = sample
        t_us.append(t)
        accel.append(tuple(values[0:3]))
        gyro.append(tuple(values[3:6]))
        if len(values) >= 7:
            temp.append(values[6])
    return t_us, accel, gyro, temp


def pose_from_name(path):
    """(axis index, +1 up / -1 down) from a file name like ...-xup-...csv."""
    match = POSE_RE.search(pathlib.Path(path).name)
    if match is None:
        raise ValueError(f"no pose (xup, ydown, ...) in file name: {path}")
    return AXES.index(match.group(1)), 1 if match.group(2) == "up" else -1


def pose_mean(accel, gyro, trim_s, motion_threshold, motion_pad_s):
    """Mean accel of the still part of one pose, and how many samples used.
    Drops trim_s from each end, then anything the gyro says was moving."""
    trim = int(trim_s * SAMPLE_RATE_HZ)
    accel = accel[trim:len(accel) - trim]
    gyro = gyro[trim:len(gyro) - trim]
    if len(accel) < 2:
        raise ValueError("capture too short after trimming")
    keep = [True] * len(accel)
    if motion_threshold > 0:
        keep = motion_mask(gyro, motion_threshold,
                           int(motion_pad_s * SAMPLE_RATE_HZ))
    kept = [a for a, k in zip(accel, keep) if k]
    if len(kept) < 2:
        raise ValueError("nothing left after excluding motion")
    return tuple(statistics.fmean(c) for c in zip(*kept)), len(kept)


def check_pose(mean, axis, sign):
    """Problems if the axis reading the most gravity isn't the labelled one."""
    strongest = max(range(3), key=lambda i: abs(mean[i]))
    actual_sign = 1 if mean[strongest] > 0 else -1
    if strongest == axis and actual_sign == sign:
        return []
    return [f"file says {pose_name(axis, sign)} but board reads "
            f"{pose_name(strongest, actual_sign)}"]


def fit_six_position(means):
    """Per-axis (offset, scale) from {(axis, sign): mean accel counts}."""
    missing = [pose_name(a, s) for a in range(3) for s in (1, -1)
               if (a, s) not in means]
    if missing:
        raise ValueError("missing poses: " + ", ".join(missing))
    offset, scale = [], []
    for axis in range(3):
        up = means[(axis, 1)][axis]
        down = means[(axis, -1)][axis]
        offset.append((up + down) / 2)
        scale.append((up - down) / 2)
    return offset, scale


def correct(raw, offset, scale):
    return tuple((r - o) / s for r, o, s in zip(raw, offset, scale))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("captures", type=pathlib.Path, nargs="+")
    parser.add_argument("--trim", type=float, default=2.0,
                        help="seconds dropped from each end of every capture")
    parser.add_argument("--motion-threshold", type=float, default=100.0,
                        help="gyro counts off median that count as motion, "
                             "0 = off")
    parser.add_argument("--motion-pad", type=float, default=0.5,
                        help="seconds dropped either side of motion")
    parser.add_argument("--out", type=pathlib.Path, default=None,
                        help="write the fit as JSON to this path")
    args = parser.parse_args()

    means, poses, problems, temps = {}, {}, [], []
    for path in args.captures:
        axis, sign = pose_from_name(path)
        if (axis, sign) in means:
            raise SystemExit(f"two captures for {pose_name(axis, sign)}")
        with open(path) as f:
            t_us, accel, gyro, temp = load_capture(f)
        mean, used = pose_mean(accel, gyro, args.trim, args.motion_threshold,
                               args.motion_pad)
        problems += check_pose(mean, axis, sign)
        means[(axis, sign)] = mean
        info = {"source": path.as_posix(), "samples": len(accel),
                "used": used, "mean_counts": list(mean)}
        if len(temp) == len(accel):
            c = statistics.fmean(temp_counts_to_c(v) for v in temp)
            info["temp_c_mean"] = c
            temps.append(c)
        poses[pose_name(axis, sign)] = info

    if problems:
        raise SystemExit("pose check failed:\n  " + "\n  ".join(problems))
    try:
        offset, scale = fit_six_position(means)
    except ValueError as e:
        raise SystemExit(str(e))

    print(f"{'pose':6} {'used':>6}   mean ax/ay/az counts")
    for name, info in poses.items():
        print(f"{name:6} {info['used']:6}   "
              + "  ".join(f"{v:9.1f}" for v in info["mean_counts"]))
    print()
    for i in range(3):
        print(f"a{AXES[i]}: offset {offset[i]:8.1f} counts "
              f"({offset[i] / ACCEL_LSB_PER_G * 1000:+.1f} mg)  "
              f"scale {scale[i]:8.1f} counts/g "
              f"({(scale[i] / ACCEL_LSB_PER_G - 1) * 100:+.2f}% vs nominal)")
    if temps:
        print(f"die temp: {min(temps):.2f} .. {max(temps):.2f} C")

    # After correction the labelled axis reads exactly +/-1 g by construction.
    # The other two axes and |g| are the independent check.
    print(f"\n{'pose':6} {'|g|':>8} {'cross-axis g':>22} {'tilt deg':>9}")
    for (axis, sign), m in means.items():
        g = correct(m, offset, scale)
        norm = math.sqrt(sum(v * v for v in g))
        cross = [g[i] for i in range(3) if i != axis]
        tilt = math.degrees(math.acos(min(1.0, abs(g[axis]) / norm)))
        info = poses[pose_name(axis, sign)]
        info.update(corrected_g=list(g), norm_g=norm, tilt_deg=tilt)
        print(f"{pose_name(axis, sign):6} {norm:8.4f} "
              + " ".join(f"{v:+10.4f}" for v in cross) + f" {tilt:9.2f}")

    if args.out:
        fit = {
            "offset_counts": offset,
            "scale_counts_per_g": scale,
            "lsb_per_g_nominal": ACCEL_LSB_PER_G,
            "poses": poses,
            "trim_s": args.trim,
            "motion_threshold_counts": args.motion_threshold,
        }
        args.out.write_text(json.dumps(fit, indent=2) + "\n")
        print(f"\nwrote {args.out}")


if __name__ == "__main__":
    main()
