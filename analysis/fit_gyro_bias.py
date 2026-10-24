"""Fit the gyro bias from a stationary IMU capture.

Usage:
    python analysis/fit_gyro_bias.py data/calibration/gyro-bias-still-20261021.csv
    python analysis/fit_gyro_bias.py CAPTURE.csv --out data/calibration/gyro-bias.json

The board must be taped flat and untouched for the whole capture. Bias is the
per-axis mean of the raw gyro counts. To show whether the bias holds steady
within the run, the capture is also split into fixed windows (default 60 s)
and the spread of the window means is reported. Captures that include the
die temperature column also report temperature per window, so bias drift can
be checked against warm-up.
"""

import argparse
import json
import pathlib
import statistics

from capture_imu import parse_sample_line

GYRO_LSB_PER_DPS = 131.0  # +/-250 dps full scale (GYRO_FS_SEL = 0)
SAMPLE_RATE_HZ = 200
TEMP_ROOM_OFFSET_COUNTS = 21  # ICM-20948 datasheet section 3.4
TEMP_COUNTS_PER_C = 333.87


def temp_counts_to_c(counts):
    return (counts - TEMP_ROOM_OFFSET_COUNTS) / TEMP_COUNTS_PER_C + 21.0


def load_capture(lines):
    """Returns (t_us list, list of (gx, gy, gz), temp counts list) from
    capture CSV lines. temp is empty for captures without a temp column."""
    t_us, gyro, temp = [], [], []
    for line in lines:
        sample = parse_sample_line(line)
        if sample is None:
            continue
        _, t, values = sample
        t_us.append(t)
        gyro.append(tuple(values[3:6]))
        if len(values) == 7:
            temp.append(values[6])
    return t_us, gyro, temp


def fit_bias(gyro):
    axes = list(zip(*gyro))
    bias = [statistics.fmean(a) for a in axes]
    return {
        "samples": len(gyro),
        "bias_counts": bias,
        "bias_dps": [b / GYRO_LSB_PER_DPS for b in bias],
        "stdev_counts": [statistics.stdev(a) for a in axes],
    }


def window_means(rows, window_len):
    """Per-column means of consecutive full windows; a partial tail is
    dropped."""
    means = []
    for start in range(0, len(rows) - window_len + 1, window_len):
        cols = zip(*rows[start:start + window_len])
        means.append([statistics.fmean(c) for c in cols])
    return means


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("capture", type=pathlib.Path)
    parser.add_argument("--window-seconds", type=float, default=60.0)
    parser.add_argument("--out", type=pathlib.Path, default=None,
                        help="write the fit as JSON to this path")
    args = parser.parse_args()

    with open(args.capture) as f:
        t_us, gyro, temp = load_capture(f)
    if len(gyro) < 2:
        raise SystemExit(f"not enough samples in {args.capture}")
    has_temp = len(temp) == len(gyro)

    fit = fit_bias(gyro)
    duration_s = (t_us[-1] - t_us[0]) / 1e6
    print(f"{fit['samples']} samples, {duration_s:.1f} s")
    for name, b, b_dps, sd in zip("xyz", fit["bias_counts"], fit["bias_dps"],
                                  fit["stdev_counts"]):
        print(f"g{name}: bias {b:8.2f} counts ({b_dps:+.4f} dps)  "
              f"noise stdev {sd:.2f} counts")

    temp_c = []
    if has_temp:
        temp_c = [temp_counts_to_c(c) for c in temp]
        fit["temp_c_mean"] = statistics.fmean(temp_c)
        fit["temp_c_range"] = [min(temp_c), max(temp_c)]
        print(f"die temp: mean {fit['temp_c_mean']:.2f} C  "
              f"range {min(temp_c):.2f} .. {max(temp_c):.2f} C")
    else:
        print("die temp: not in this capture")

    window_len = int(args.window_seconds * SAMPLE_RATE_HZ)
    windows = window_means(gyro, window_len)
    temp_windows = window_means([(c,) for c in temp_c], window_len)
    columns = "gx gy gz counts" + (", die temp C" if has_temp else "")
    print(f"\n{len(windows)} windows of {args.window_seconds:g} s "
          f"({columns}):")
    for i, m in enumerate(windows):
        line = f"  {i}: " + "  ".join(f"{v:8.2f}" for v in m)
        if has_temp:
            line += f"   {temp_windows[i][0]:6.2f}"
        print(line)
    if len(windows) > 1:
        spread = [max(a) - min(a) for a in zip(*windows)]
        print("  spread (max-min): "
              + "  ".join(f"{v:8.2f}" for v in spread) + " counts")
        fit["window_seconds"] = args.window_seconds
        fit["window_spread_counts"] = spread

    if args.out:
        fit["source"] = args.capture.as_posix()
        args.out.write_text(json.dumps(fit, indent=2) + "\n")
        print(f"\nwrote {args.out}")


if __name__ == "__main__":
    main()
