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
queued and streamed over USB as CSV (`seq,t_us,ax,ay,az,gx,gy,gz,temp`; `temp`
is the raw die temperature, read in the same burst, and is absent from
captures before 2026-10-24). The IMU
runs at its full internal rate (~1.1 kHz, DLPF ~51 Hz) so each timer read gets
fresh data. `analysis/capture_imu.py` captures the stream to `data/raw/` and
reports missed samples and interval jitter. Verification run (60 s, board at
rest): 12000 samples, 0 missed, every interval exactly 5000 us. Note `t_us` is
the timer-fire time, not the I2C read-completion time.

Gyro bias: `analysis/fit_gyro_bias.py` fits per-axis bias, noise, and
per-minute window means (plus die temp when logged) from a stationary
capture. Three 5-minute stationary captures so far (`data/calibration/`)
disagree: gx is steady (~25-26 counts), but gy ranged 60.6-73.2 and gz
-1.1-5.4 counts across runs. A capture right after power-on drifted steadily
within the run; a back-to-back warm one was flat. Warm-up is the suspected
cause but is unconfirmed — the next step is a cold-start capture with
temperature logged. Don't pick a final bias value until that's understood.

Next: settle gyro-bias stability vs. temperature, then 6-position accel,
then the magnetometer (AK09916, not yet brought up). No SD card yet, so
captures go over USB.
