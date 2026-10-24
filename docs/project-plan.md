# Project Plan

The end goal is one number: how much optical-flow fusion reduces
return-to-origin position error compared to IMU-only dead reckoning, measured
over repeated cart runs on a taped floor course. This is as much a measurement
project as a firmware one, so the number has to come from real hardware runs.

## Hardware

| Component | Part | Interface |
|---|---|---|
| MCU | SparkFun Thing Plus ESP32-S3 (WRL-24408) | Qwiic I2C on IO8 (SDA) / IO9 (SCL) |
| IMU | ICM-20948 9-DoF (Adafruit 4554) | I2C, 0x69 |
| Rangefinder | VL53L1X ToF (Adafruit 3967) | I2C, 0x29 |
| Optical flow | PMW3901 (Phase 4, not yet bought) | SPI |
| Storage | Onboard microSD | logging |

## Phases

0. Toolchain, register-map study, architecture decisions
1. IMU bring-up and calibration (gyro bias, 6-position accel, mag hard/soft iron)
2. Orientation estimation: complementary filter first, then Kalman
3. Dead reckoning with drift characterization
4. Optical flow fusion, compared against Phase 3 on the same rig
5. Packaging and write-up

Phases go in order. Dead reckoning doesn't start until orientation has been
validated against logged data.

## Rules

- **Raw registers only.** No DMP or on-chip fused orientation. Writing the
  fusion filter is the point.
- **Select the bank on every access.** The ICM-20948 splits its registers
  across four banks. Every read and write selects its bank first, never
  relying on the bank left over from the last access.
- **Timer-driven sampling.** Samples come from a hardware timer interrupt at
  a fixed rate and are timestamped. No `delay()` loops, since jitter corrupts
  integration.
- **Calibration is offline.** Gyro bias, accel, and mag calibration are fit
  in Python from logged data and stored in flash, not recomputed at boot.
- **Analysis is offline.** Ellipsoid fits and other analysis run in Python.
  The firmware samples, timestamps, logs, and (from Phase 2) runs the filter.
- **Validate orientation before dead reckoning.** Gravity leaking through a
  bad orientation estimate silently ruins every position estimate after it,
  so it has to be caught first.
- **No made-up numbers.** Every drift rate, error, and improvement comes from
  logged hardware runs. If the data doesn't exist yet, the result is TBD.

## Layout

- `firmware/`: PlatformIO/Arduino project for the ESP32-S3
- `analysis/`: Python capture, calibration, and analysis scripts
- `data/`: logged captures and run results
- `docs/`: design notes and register-map notes

## Status

**Phase 0:** done.

**Phase 1:** in progress.
- Driver: bank select, `WHO_AM_I` (`0xEA`), wake sequence, and accel/gyro/temp
  burst read all work on hardware (`firmware/lib/icm20948/`).
- Sampling: a 200 Hz hardware timer timestamps each tick in its ISR and
  wakes a sampler task that does the I2C read. Samples stream over USB as
  CSV (`seq,t_us,ax,ay,az,gx,gy,gz,temp`; captures before 2026-10-24 have no
  `temp`). The IMU runs at ~1.1 kHz with the DLPF at ~51 Hz so each read is
  fresh. `t_us` is the timer-fire time, not the read time.
- Capture: `analysis/capture_imu.py` saves the stream and reports missed
  samples and jitter. A 60 s test at rest gave 12000 samples, 0 missed, every
  interval 5000 us.
- Gyro bias: `analysis/fit_gyro_bias.py` fits bias, noise, and per-minute
  drift (plus die temp when logged). Three 5-minute stationary captures
  disagree: gx is steady at ~25-26 counts, but gy ranged 60.6-73.2 and gz
  -1.1-5.4. The capture right after power-on drifted steadily, while the warm
  one right after it was flat. Warm-up is the likely cause but isn't confirmed
  yet. Next is a cold-start capture with temperature logged. No final bias
  value until that's understood.

**Next:** gyro bias vs. temperature, then 6-position accel, then the
magnetometer (AK09916). No SD card yet, so captures go over USB.
