#pragma once

#include <cstddef>
#include <cstdint>
#include "CombineBigEndian.h"

// AK09916 data registers are low byte first (accel/gyro are high first).
inline int16_t combineLittleEndian(uint8_t low, uint8_t high) {
    return static_cast<int16_t>(static_cast<uint16_t>(high) << 8 | low);
}

// I2C_PERIPHx_ADDR: [7] read (1) / write (0), [6:0] peripheral address.
inline uint8_t i2cPeriphAddrValue(uint8_t address, bool read) {
    return static_cast<uint8_t>((read ? 0x80 : 0x00) | (address & 0x7F));
}

// I2C_PERIPHx_CTRL: [7] enable, [3:0] bytes to read.
inline uint8_t i2cPeriphCtrlValue(bool enable, uint8_t length) {
    return static_cast<uint8_t>((enable ? 0x80 : 0x00) | (length & 0x0F));
}

// Bank 0 burst from ACCEL_XOUT_H: 14 bytes accel/gyro/temp, then the 9
// bytes peripheral 0 copies from the AK09916 (ST1 .. ST2) into
// EXT_PERIPH_SENS_DATA_00.
constexpr size_t ACCEL_GYRO_TEMP_LEN = 14;
constexpr size_t MAG_READ_LEN = 9;
constexpr size_t SENSOR_BLOCK_LEN = ACCEL_GYRO_TEMP_LEN + MAG_READ_LEN;

// ax, ay, az, gx, gy, gz, temp, mx, my, mz
constexpr size_t SAMPLE_RAW_WORDS = 10;

// magStatus is ST2 << 8 | ST1 (ST1 bit 0 DRDY, bit 1 DOR; ST2 bit 3 HOFL).
inline void decodeSensorBlock(const uint8_t block[SENSOR_BLOCK_LEN],
                              int16_t raw[SAMPLE_RAW_WORDS],
                              uint16_t &magStatus) {
    for (size_t w = 0; w < 7; w++) {
        raw[w] = combineBigEndian(block[w * 2], block[w * 2 + 1]);
    }
    const uint8_t *mag = block + ACCEL_GYRO_TEMP_LEN;
    for (size_t a = 0; a < 3; a++) {
        raw[7 + a] = combineLittleEndian(mag[1 + a * 2], mag[2 + a * 2]);
    }
    magStatus = static_cast<uint16_t>(mag[8] << 8 | mag[0]);
}
