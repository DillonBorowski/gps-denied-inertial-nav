# CLAUDE.md

This file gives Claude Code (and any other agent working in this repo) the
context and constraints needed to work on this project correctly.

## Project

GPS-Denied Inertial Navigation — an embedded firmware portfolio project.

The goal is to build IMU-based sensor fusion and Kalman filtering on
embedded C, culminating in a quantified deliverable: the reduction in
return-to-origin position error, measured across repeated cart runs on a
taped floor course, expressed as a percentage improvement when fusing
optical flow versus IMU-only dead reckoning.

This is a measurement project as much as a firmware project. The entire
point is to produce a real, honest, reproducible number from real hardware
runs — not a simulated or estimated one.

## Hardware

| Component | Part | Interface |
|---|---|---|
| MCU | SparkFun Thing Plus ESP32-S3 (WRL-24408) | Qwiic I2C on IO8 (SDA) / IO9 (SCL) |
| IMU | ICM-20948 9-DoF (Adafruit 4554) | I2C, address 0x69 |
| Rangefinder | VL53L1X time-of-flight (Adafruit 3967) | I2C, address 0x29 |
| Optical flow | PMW3901 (not yet acquired — Phase 4 only) | SPI |
| Storage | Onboard microSD | for data logging |

## Phases

0. Toolchain, register-map study, architecture decisions
1. IMU bring-up and calibration (gyro bias, 6-position accel, mag hard/soft iron)
2. Orientation estimation — complementary filter first, then Kalman
3. Dead reckoning with quantified drift characterization
4. Optical flow fusion, paired before/after against Phase 3 on an identical rig
5. Repo packaging and write-up

Work should generally proceed in phase order. Don't start Phase 3 (dead
reckoning) work until Phase 2 (orientation) has been validated against
logged data — see constraints below.

## Standing constraints

These are non-negotiable project rules, not stylistic defaults. Follow them
even if a shortcut looks tempting.

- **Raw registers only.** Use raw ICM-20948 sensor registers exclusively.
  Never use the onboard DMP or any pre-fused orientation output from the
  chip. Authoring the sensor fusion filter ourselves is the point of the
  project — using the DMP would defeat it.

- **Explicit bank selection.** The ICM-20948 pages its registers across four
  register banks. Every register access (read or write) must explicitly
  select the correct bank first. Never assume the bank is already correct
  from a prior access.

- **Timer-interrupt-driven sampling.** IMU sampling must be driven by a
  hardware timer interrupt at a fixed rate, with every sample timestamped.
  Never use delay-based loops (e.g. `delay()` + read) for sampling — this
  introduces jitter that corrupts integration.

- **Calibration is offline.** Calibration coefficients (gyro bias, 6-position
  accelerometer calibration, magnetometer hard/soft iron) are computed
  offline in Python from logged data, then stored in flash. Never
  recompute calibration at boot.

- **Offline analysis.** Ellipsoid fitting and other calibration/analysis
  math happens offline in Python, not on-device. Firmware's job is to
  sample, timestamp, log, and (from Phase 2 on) run the validated filter —
  not to do exploratory math.

- **Validate orientation before dead reckoning.** Orientation estimation
  must be validated against logged data before any dead-reckoning work
  begins. Uncorrected gravity leakage in the orientation estimate corrupts
  all downstream position estimates silently — this is the most likely
  source of a subtly wrong final number, so it must be caught early.

- **Never fabricate results.** All reported numbers — drift rates, position
  errors, percentage improvements — come from logged data on real hardware
  runs. Never estimate, simulate, or interpolate a headline result. If data
  isn't available yet, say so; don't fill the gap with a plausible-looking
  number.

## Repository layout

- `firmware/` — embedded C source (PlatformIO/Arduino project for the
  ESP32-S3)
- `analysis/` — offline Python scripts (calibration fitting, drift
  characterization, plotting, before/after comparison)
- `data/` — logged datasets from hardware runs (raw logs, calibration
  captures, run results)
- `docs/` — design notes, register-map notes, architecture decisions,
  write-up

## Current status

Phase 0 complete — repository scaffolding, PlatformIO toolchain, and
ICM-20948 register-map study done.

Phase 1 in progress. Hardware is wired (ESP32-S3 Thing Plus <-> ICM-20948
over Qwiic I2C) and verified: bank-select mechanism, `WHO_AM_I` read
(confirmed `0xEA`), the `PWR_MGMT_1`/`PWR_MGMT_2` wake sequence, and a raw
accel/gyro burst read are all implemented and producing sane values on real
hardware. See `firmware/lib/icm20948/` for the driver and
`firmware/lib/icm20948_bitops/` for the unit-tested pure bit-math helpers.

Timer-interrupt-driven sampling is in place and verified on hardware
(`firmware/src/main.cpp`): a hardware timer at 200 Hz timestamps each tick in
its ISR and wakes a sampler task that does the I2C burst read; samples are
queued and streamed over USB as CSV
(`seq,t_us,ax,ay,az,gx,gy,gz,temp,mx,my,mz,mstat`). `temp` is the raw die
temperature and is absent from captures before 2026-10-24. `mx,my,mz` are
raw AK09916 counts (0.15 uT/LSB) and `mstat` is `ST2 << 8 | ST1`; both are
absent before 2026-10-30. All of it comes from one 23-byte burst read. The
IMU
runs at its full internal rate (~1.1 kHz, DLPF ~51 Hz) so each timer read gets
fresh data. `analysis/capture_imu.py` captures the stream to `data/raw/` and
reports missed samples and interval jitter. On connect it sends `i`; the
firmware replies with info lines (WHO_AM_I, rate, gyro bias and accel cal in
flash, uptime) ending in `# end info`, and the capture writes them as `#`
lines at the top of the CSV (captures from 2026-10-30 on). Readers must skip
`#` lines. Verification run (60 s, board at
rest): 12000 samples, 0 missed, every interval exactly 5000 us. Note `t_us` is
the timer-fire time, not the I2C read-completion time.

Gyro bias: `analysis/fit_gyro_bias.py` fits per-axis bias, noise, and
per-minute window means (plus die temp when logged) from a stationary
capture, and cuts out bumps automatically (any axis >100 counts off its
median, +/-0.5 s). Captures are in `data/calibration/`.

- After power-on the bias settles over ~5 minutes, then holds within ~1
  count. Seen on both cold starts (2026-10-24): gy fell ~5 counts before
  flattening. The 10-min cold-start capture (`gyro-bias-cold-20261024.csv`)
  logged temp: the die only warmed 21.8 -> 22.3 C, so it can't separate
  thermal drift from power-on settling. Either way: wait >= 5 min after
  power-on before any capture or run.
- Settled bias on 2026-10-24: ~gx 22.5 / gy 67.4 / gz 2.0 counts at 22.2 C.
  The 2026-10-21 run gave gx 26.1 / gy 60.6 / gz -1.1 (no temp logged), so
  day-to-day bias differs by up to ~7 counts (~0.05 dps). Unexplained. The
  board was taped down more firmly on 10/24, and mounting stress can shift
  MEMS bias, but that's unverified.
- Decision (2026-10-30): use a single flash-stored bias for now. Settled fit
  (cold-start capture, first 300 s skipped) is gx 22.61 / gy 67.45 / gz 2.05
  counts, saved in `data/calibration/gyro-bias.json` and compiled into
  `firmware/lib/calibration/GyroBias.h`. Firmware prints it as a `#` line at
  startup; the CSV stream stays raw, so switching to a per-run stationary
  segment later (fit offline from each run's own log) is still possible.
  Whatever is chosen, IMU-only and fused runs must use the same bias method.

Accel 6-position (2026-10-30): `analysis/fit_accel_6pos.py` fits per-axis
offset and scale from six 45 s stationary captures
(`data/calibration/accel-6pos-{x,y,z}{up,down}-20261030.csv`, pose read from
the file name and checked against the data). Result in
`data/calibration/accel-6pos.json` and `firmware/lib/calibration/AccelCal.h`:
offset x +4.4 / y -14.8 / z -7.5 mg, scale +0.20 / +0.23 / +0.41% vs nominal
16384 counts/g. Corrected |g| is 1.0000-1.0005 across all six poses, so
cross-axis misalignment looks small. Die temp rose 23.2 -> 26.6 C during the
session (handling); temperature dependence of the offset is unchecked.
- Setting poses: the IMU breakout has to be taped to a block with the Qwiic
  cable taped down, or it tips onto an edge. Z points out of the chip side.
  Check each pose (and tilt < ~2 deg) with a live accel readout before
  capturing.

Magnetometer (2026-10-30): AK09916 brought up through the ICM-20948's I2C
master and streaming (WIA2 0x09, ~97 new readings/s at 100 Hz continuous).
Setup and gotchas are in `docs/icm20948-register-map.md`. The firmware now
does a full IMU reset at boot, because reflashing the ESP32 doesn't reset
the IMU and old register state was leaking into new runs. At rest the field
reads ~93 uT, above Earth's 25-65 uT, so expect a large hard-iron offset.
Mag axes not yet checked against accel/gyro axes; not calibrated yet.

Open issue: accel Z offset shifted by +180 counts (10.9 mg) on 2026-10-30,
after the 6-position calibration. Z up now reads 1.0113 g and Z down 0.9895 g
with the flash cal; re-fitting only the Z offset (-122.8 -> ~+56.8) gives
1.0003 g for both, so it is an offset shift and the scale is unchanged. The
flash value has NOT been updated. The shift appeared across a USB disconnect
during which the board was moved and left untaped. Ruled out: mag on/off
(A/B), firmware (old commit 71ae4eb reads the same), power cycling (no change
across a replug), re-taping (didn't restore it). Best guess: mechanical stress
on the breakout (cable pull), unconfirmed. Takeaways: the IMU needs a rigid,
repeatable mount with the cable strain-relieved; calibrate on the final
mount; and each run should start with a few seconds at rest, checking |g|
is close to 1.000 so a shifted offset gets caught.

Next: find the accel shift cause, check mag axis directions, then a mag
calibration capture (ellipsoid fit, hard + soft iron). No SD card yet, so
captures go over USB.
