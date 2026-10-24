"""Fit gyro bias from a stationary capture.

Usage:
    python analysis/fit_gyro_bias.py CAPTURE.csv
    python analysis/fit_gyro_bias.py CAPTURE.csv --out data/calibration/gyro-bias.json

Board taped flat and untouched. Also prints per-minute means (and temp if
logged) to check drift. Bumps are cut out automatically (--motion-threshold).
"""

import argparse
import json
import pathlib
import statistics

from capture_imu import parse_sample_line

GYRO_LSB_PER_DPS = 131.0  # +/-250 dps
SAMPLE_RATE_HZ = 200
TEMP_ROOM_OFFSET_COUNTS = 21  # datasheet 3.4
TEMP_COUNTS_PER_C = 333.87


def temp_counts_to_c(counts):
    return (counts - TEMP_ROOM_OFFSET_COUNTS) / TEMP_COUNTS_PER_C + 21.0


def load_capture(lines):
    """(t_us, gyro xyz tuples, temp counts). temp is empty for old captures."""
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


def motion_mask(gyro, threshold, pad):
    """True for samples to keep. Drops anything more than threshold counts
    off the median on any axis, plus pad samples either side."""
    medians = [statistics.median(a) for a in zip(*gyro)]
    moving = [any(abs(v - m) > threshold for v, m in zip(g, medians))
              for g in gyro]
    keep = [True] * len(gyro)
    for i, m in enumerate(moving):
        if m:
            for j in range(max(0, i - pad), min(len(gyro), i + pad + 1)):
                keep[j] = False
    return keep


def window_means(rows, window_len, keep=None):
    """Mean of each full window; leftover samples are dropped. With keep,
    only kept rows count, and a window under half kept is None."""
    means = []
    for start in range(0, len(rows) - window_len + 1, window_len):
        idx = range(start, start + window_len)
        if keep is not None:
            idx = [i for i in idx if keep[i]]
            if len(idx) < window_len / 2:
                means.append(None)
                continue
        cols = zip(*(rows[i] for i in idx))
        means.append([statistics.fmean(c) for c in cols])
    return means


def excluded_spans(t_s, keep):
    """(start, end) seconds of each dropped stretch."""
    spans = []
    for t, k in zip(t_s, keep):
        if k:
            continue
        if spans and t - spans[-1][1] <= 1.5 / SAMPLE_RATE_HZ:
            spans[-1][1] = t
        else:
            spans.append([t, t])
    return spans


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("capture", type=pathlib.Path)
    parser.add_argument("--window-seconds", type=float, default=60.0)
    parser.add_argument("--skip-start", type=float, default=0.0,
                        help="seconds to drop from the start")
    parser.add_argument("--motion-threshold", type=float, default=100.0,
                        help="counts off median that count as motion, 0 = off")
    parser.add_argument("--motion-pad", type=float, default=0.5,
                        help="seconds dropped either side of motion")
    parser.add_argument("--out", type=pathlib.Path, default=None,
                        help="write the fit as JSON to this path")
    args = parser.parse_args()

    with open(args.capture) as f:
        t_us, gyro, temp = load_capture(f)
    if len(gyro) < 2:
        raise SystemExit(f"not enough samples in {args.capture}")
    has_temp = len(temp) == len(gyro)
    t_s = [(t - t_us[0]) / 1e6 for t in t_us]

    keep = [t >= args.skip_start for t in t_s]
    if args.motion_threshold > 0:
        pad = int(args.motion_pad * SAMPLE_RATE_HZ)
        still = motion_mask(gyro, args.motion_threshold, pad)
        keep = [a and b for a, b in zip(keep, still)]
    kept = [g for g, k in zip(gyro, keep) if k]
    if len(kept) < 2:
        raise SystemExit("nothing left after excluding motion")

    fit = fit_bias(kept)
    print(f"{len(gyro)} samples, {t_s[-1]:.1f} s, {len(kept)} used")
    spans = excluded_spans(t_s, keep)
    if spans:
        print("excluded: " + ", ".join(f"{a:.1f}-{b:.1f} s" for a, b in spans))
    for name, b, b_dps, sd in zip("xyz", fit["bias_counts"], fit["bias_dps"],
                                  fit["stdev_counts"]):
        print(f"g{name}: bias {b:8.2f} counts ({b_dps:+.4f} dps)  "
              f"noise stdev {sd:.2f} counts")

    temp_c = []
    if has_temp:
        temp_c = [temp_counts_to_c(c) for c in temp]
        kept_c = [c for c, k in zip(temp_c, keep) if k]
        fit["temp_c_mean"] = statistics.fmean(kept_c)
        fit["temp_c_range"] = [min(kept_c), max(kept_c)]
        print(f"die temp: mean {fit['temp_c_mean']:.2f} C  "
              f"range {min(kept_c):.2f} .. {max(kept_c):.2f} C")
    else:
        print("die temp: not in this capture")

    window_len = int(args.window_seconds * SAMPLE_RATE_HZ)
    windows = window_means(gyro, window_len, keep)
    temp_windows = window_means([(c,) for c in temp_c], window_len, keep)
    columns = "gx gy gz counts" + (", die temp C" if has_temp else "")
    print(f"\n{len(windows)} windows of {args.window_seconds:g} s "
          f"({columns}):")
    for i, m in enumerate(windows):
        if m is None:
            print(f"  {i}: (mostly excluded)")
            continue
        line = f"  {i}: " + "  ".join(f"{v:8.2f}" for v in m)
        if has_temp:
            line += f"   {temp_windows[i][0]:6.2f}"
        print(line)
    used = [m for m in windows if m is not None]
    if len(used) > 1:
        spread = [max(a) - min(a) for a in zip(*used)]
        print("  spread (max-min): "
              + "  ".join(f"{v:8.2f}" for v in spread) + " counts")
        fit["window_seconds"] = args.window_seconds
        fit["window_spread_counts"] = spread

    if args.out:
        fit["source"] = args.capture.as_posix()
        fit["skip_start_s"] = args.skip_start
        fit["motion_threshold_counts"] = args.motion_threshold
        fit["excluded_s"] = spans
        args.out.write_text(json.dumps(fit, indent=2) + "\n")
        print(f"\nwrote {args.out}")


if __name__ == "__main__":
    main()
