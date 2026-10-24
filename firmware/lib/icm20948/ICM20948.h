#pragma once

#include <Wire.h>
#include <cstdint>

// Raw-register ICM-20948 driver. No DMP; every access selects its bank.
class ICM20948 {
public:
    void begin(TwoWire &wire, uint8_t address);

    // Selects the bank, then reads one register.
    uint8_t readRegister(uint8_t bank, uint8_t reg);

    // Selects the bank, then writes one register.
    void writeRegister(uint8_t bank, uint8_t reg, uint8_t value);

    // WHO_AM_I (bank 0, 0x00), should be 0xEA.
    uint8_t readWhoAmI();

    // Clears SLEEP and enables all accel/gyro axes.
    void wake();

    // Full internal rate (~1.1 kHz), DLPF ~51 Hz, +/-250 dps, +/-2 g.
    // Running faster than the 200 Hz timer keeps each read fresh.
    void configureSampling();

    // One burst read of 0x2D-0x3A: {ax, ay, az, gx, gy, gz, temp} raw counts.
    // Temp in C = (temp - 21) / 333.87 + 21.
    void readAccelGyroTempRaw(int16_t out[7]);

private:
    void selectBank(uint8_t bank);

    TwoWire *wire_ = nullptr;
    uint8_t address_ = 0;
};
