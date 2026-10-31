#pragma once

#include <Wire.h>
#include <cstdint>
#include "MagBits.h"

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

    // Full device reset to power-on defaults, so nothing left over from a
    // previous run (the IMU isn't reset when the ESP32 is reflashed).
    void reset();

    // Clears SLEEP and enables all accel/gyro axes.
    void wake();

    // Full internal rate (~1.1 kHz), DLPF ~51 Hz, +/-250 dps, +/-2 g.
    // Running faster than the 200 Hz timer keeps each read fresh.
    void configureSampling();

    // AK09916 soft reset, continuous 100 Hz, then I2C peripheral 0 set to
    // auto-read it. Call after enableMagBus(). Returns false on a NACK.
    bool startMag();

    // One 23-byte burst from 0x2D: accel, gyro, temp, then the AK09916 data
    // peripheral 0 copied in. raw = {ax ay az gx gy gz temp mx my mz} counts,
    // magStatus = ST2 << 8 | ST1. Temp in C = (temp - 21) / 333.87 + 21.
    void readSensorBlock(int16_t raw[SAMPLE_RAW_WORDS], uint16_t &magStatus);

    // Turns on the ICM-20948 I2C master that talks to the AK09916
    // magnetometer on its internal bus. Call after wake().
    void enableMagBus();

    // One AK09916 register read/write through I2C peripheral 4. Polls for
    // completion, so setup only. Returns false on NACK or timeout.
    bool readMagRegister(uint8_t reg, uint8_t &value);
    bool writeMagRegister(uint8_t reg, uint8_t value);

private:
    bool waitPeriph4Done();
    void selectBank(uint8_t bank);

    TwoWire *wire_ = nullptr;
    uint8_t address_ = 0;
};
