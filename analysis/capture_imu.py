"""Capture the firmware's timer-driven IMU stream over USB serial to a CSV.

Usage:
    python analysis/capture_imu.py --port COM3 --seconds 60
    python analysis/capture_imu.py --port COM3 --seconds 60 --out data/raw/still.csv

Each data line from the firmware is "seq,t_us,ax,ay,az,gx,gy,gz" (raw counts,
t_us captured in the timer ISR). Lines starting with '#' are firmware status
messages and are echoed to the console, not saved. After capture, prints how
many samples were missed (gaps in seq) and the sample-interval statistics.
"""

import argparse
import datetime
import pathlib
import statistics
import time

SEQ_MODULUS = 2**32
CSV_HEADER = "seq,t_us,ax,ay,az,gx,gy,gz"
REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent


def parse_sample_line(line):
    """Returns (seq, t_us, [ax, ay, az, gx, gy, gz]), or None for anything
    that isn't a complete sample line."""
    fields = line.strip().split(",")
    if len(fields) != 8:
        return None
    try:
        values = [int(f) for f in fields]
    except ValueError:
        return None
    return values[0], values[1], values[2:]


def summarize(seqs, times_us):
    """Missed-sample count and interval stats. Intervals are only taken
    between consecutive seq numbers, so a dropped sample isn't counted as
    timing jitter."""
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
    import serial  # pyserial; imported here so the tests don't need it

    seqs, times_us = [], []
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with serial.Serial(port, 115200, timeout=1) as ser, \
            open(out_path, "w", newline="") as out:
        ser.reset_input_buffer()
        ser.readline()  # discard a possibly partial first line
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
