# GPS-Denied Inertial Navigation

IMU sensor fusion and Kalman filtering in embedded C on an ESP32-S3, built
from raw sensor registers. The question: how far can dead reckoning go
without GPS, and how much does optical-flow fusion help?

## Goal

Measure return-to-origin position error over repeated cart runs on a taped
floor course, and report the percentage improvement from fusing optical flow
versus IMU-only dead reckoning.

| Metric | Result |
|---|---|
| IMU-only return-to-origin error | TBD |
| Optical-flow-fused return-to-origin error | TBD |
| Improvement | TBD |

These come from the Phase 3 and Phase 4 hardware runs. See
[CLAUDE.md](CLAUDE.md) for the phases and rules.

## Hardware

- SparkFun Thing Plus ESP32-S3 (WRL-24408), Qwiic I2C on IO8/IO9
- ICM-20948 9-DoF IMU (Adafruit 4554), I2C 0x69
- VL53L1X time-of-flight rangefinder (Adafruit 3967), I2C 0x29
- PMW3901 optical flow sensor, SPI (Phase 4)
- Onboard microSD for logging

## Status

Phase 1 (IMU bring-up and calibration) is in progress. The raw-register
driver and 200 Hz timer-driven sampling work on hardware, and gyro bias
calibration is underway. Details are in
[CLAUDE.md](CLAUDE.md#current-status).

## Building

Firmware (PlatformIO):

```
cd firmware
pio run -e thing_plus_esp32s3 -t upload
pio test -e native
```

Capture and analysis (Python 3, `pyserial`):

```
python analysis/capture_imu.py --port COM3 --seconds 300 --out data/calibration/still.csv
python analysis/fit_gyro_bias.py data/calibration/still.csv
cd analysis && python -m unittest
```
