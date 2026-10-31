"""Capture the IMU stream over USB serial to a CSV.

Usage:
    python analysis/capture_imu.py --port COM3 --seconds 60
    python analysis/capture_imu.py --port COM3 --seconds 60 --out data/raw/still.csv

On connect it asks the board for its info lines (rate, calibration in flash)
and writes them as '#' lines at the top of the CSV. Other '#' lines from the
firmware are printed, not saved. Prints missed samples and interval stats at
the end.
"""

import argparse
import datetime
import pathlib
import statistics
import time

SEQ_MODULUS = 2**32
CSV_HEADER = "seq,t_us,ax,ay,az,gx,gy,gz,temp,mx,my,mz,mstat"
REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
INFO_REQUEST = b"i"
INFO_END = "# end info"
INFO_MAX_LINES = 400  # ~2 s of samples at 200 Hz


def parse_sample_line(line):
    """(seq, t_us, values) or None. Old captures lack temp (8 fields) or the
    mag columns (9 fields); current ones have 13."""
    fields = line.strip().split(",")
    if len(fields) not in (8, 9, 13):
        return None
    try:
        values = [int(f) for f in fields]
    except ValueError:
        return None
    return values[0], values[1], values[2:]


def collect_info(readline, max_lines):
    """'#' lines up to the end marker, or None if it never arrives
    (old firmware, or readline timed out). Sample lines are skipped."""
    info = []
    for _ in range(max_lines):
        line = readline()
        if not line:
            return None
        line = line.strip()
        if line == INFO_END:
            return info
        if line.startswith("#"):
            info.append(line)
    return None


def summarize(seqs, times_us):
    """Missed samples and interval stats (gaps don't count as jitter)."""
    missed = 0
    intervals = []
    for i in range(1, len(seqs)):
        step = (seqs[i] - seqs[i - 1]) % SEQ_MODULUS
        if step == 1:
            intervals.append(times_us[i] - times_us[i - 1])
        else:
            missed += step - 1
    summary = {"samples": len(seqs), "missed": missed}
    if intervals:
        summary["dt_min_us"] = min(intervals)
        summary["dt_max_us"] = max(intervals)
        summary["dt_mean_us"] = statistics.fmean(intervals)
        summary["dt_stdev_us"] = (
            statistics.stdev(intervals) if len(intervals) > 1 else 0.0
        )
    return summary


def default_out_path():
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    return REPO_ROOT / "data" / "raw" / f"imu-{stamp}.csv"


def capture(port, seconds, out_path):
    import serial  # here so tests don't need pyserial

    seqs, times_us = [], []
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with serial.Serial(port, 115200, timeout=1) as ser, \
            open(out_path, "w", newline="") as out:
        ser.reset_input_buffer()
        ser.write(INFO_REQUEST)
        info = collect_info(
            lambda: ser.readline().decode("ascii", errors="replace"),
            INFO_MAX_LINES)
        if info is None:
            print("# warning: no info from board (old firmware?), "
                  "calibration not recorded")
        else:
            for line in info:
                print(line)
                out.write(line + "\n")
        ser.readline()  # skip partial line
        out.write(CSV_HEADER + "\n")
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            line = ser.readline().decode("ascii", errors="replace")
            if line.startswith("#"):
                print(line.rstrip())
                continue
            sample = parse_sample_line(line)
            if sample is None:
                continue
            seq, t_us, _ = sample
            seqs.append(seq)
            times_us.append(t_us)
            out.write(line.strip() + "\n")
    return summarize(seqs, times_us)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", required=True, help="serial port, e.g. COM3")
    parser.add_argument("--seconds", type=float, required=True)
    parser.add_argument("--out", type=pathlib.Path, default=None)
    args = parser.parse_args()

    out_path = args.out or default_out_path()
    summary = capture(args.port, args.seconds, out_path)
    print(f"saved {summary['samples']} samples to {out_path}")
    print(f"missed samples: {summary['missed']}")
    if "dt_mean_us" in summary:
        print(
            "interval us: mean {dt_mean_us:.1f}  stdev {dt_stdev_us:.1f}  "
            "min {dt_min_us}  max {dt_max_us}".format(**summary)
        )


if __name__ == "__main__":
    main()
