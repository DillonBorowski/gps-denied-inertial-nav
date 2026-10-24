#pragma once

#include <Wire.h>
#include <cstdint>

// Raw-register ICM-20948 driver. Per CLAUDE.md: no DMP, explicit bank
// selection on every access, no implicit bank caching between calls.
class ICM20948 {
public:
    void begin(TwoWire &wire, uint8_t address);

    // Selects the given bank, then reads one register.
    uint8_t readRegister(uint8_t bank, uint8_t reg);

    // Selects the given bank, then writes one register.
    void writeRegister(uint8_t bank, uint8_t reg, uint8_t value);

    // WHO_AM_I (bank 0, 0x00). Expect 0xEA per
    // docs/icm20948-register-map.md.
    uint8_t readWhoAmI();

    // Clears SLEEP and enables all accel/gyro axes (PWR_MGMT_1/2, bank 0).
    // Must be called before readAccelGyroTempRaw() will return live data.
    void wake();

    // Bank 2 output config for timer-driven sampling: ODR alignment on, then
    // gyro and accel at their full internal rate (~1.1 kHz, dividers 0) with
    // the DLPF at ~51 Hz bandwidth, ranges +/-250 dps and +/-2 g. The MCU
    // timer samples well below this rate, so each read returns data at most
    // ~0.9 ms old, and the DLPF keeps content below that rate's Nyquist.
    void configureSampling();

    // Burst-reads the 14-byte ACCEL_XOUT_H..TEMP_OUT_L block (bank 0,
    // 0x2D-0x3A) in one I2C transaction and fills out[0..6] with
    // {ax, ay, az, gx, gy, gz, temp} as raw signed counts. Die temperature in
    // degC is approximately (temp - 21) / 333.87 + 21 (datasheet section 3.4).
    void readAccelGyroTempRaw(int16_t out[7]);

private:
    void selectBank(uint8_t bank);

    TwoWire *wire_ = nullptr;
    uint8_t address_ = 0;
};
