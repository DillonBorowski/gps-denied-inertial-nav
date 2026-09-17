# GPS-Denied Inertial Navigation

IMU sensor fusion and Kalman filtering in embedded C, built from raw sensor
registers on an ESP32-S3. This is a portfolio project exploring how far
dead-reckoning navigation can go without GPS, and how much optical-flow
fusion improves it.

## Goal

Quantify return-to-origin position error across repeated cart runs on a
taped floor course, and report the percentage improvement from fusing
optical-flow velocity into the estimate versus IMU-only dead reckoning.

## Hardware

- SparkFun Thing Plus ESP32-S3 (WRL-24408) — Qwiic I2C on IO8 (SDA) / IO9 (SCL)
- ICM-20948 9-DoF IMU (Adafruit 4554) — I2C, address 0x69
- VL53L1X time-of-flight rangefinder (Adafruit 3967) — I2C, address 0x29
- PMW3901 optical flow sensor — SPI (not yet acquired; Phase 4)
- Onboard microSD for data logging

## Deliverable metric

IMU-only dead reckoning return-to-origin error: **TBD**
Optical-flow-fused return-to-origin error: **TBD**
Improvement: **TBD%**

Numbers are pending Phase 3 (dead-reckoning baseline) and Phase 4 (fused
comparison) hardware runs — see [CLAUDE.md](CLAUDE.md) for the full project
phase plan and engineering constraints.

## Status

Phase 0 complete — scaffolding, toolchain, and register-map study done.
No firmware logic written yet.

**Blocked on hardware** — Phase 1 needs the ESP32-S3 and ICM-20948
physically connected. Resume once that hardware is on hand.
