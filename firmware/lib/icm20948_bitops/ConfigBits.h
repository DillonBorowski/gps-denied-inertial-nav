#pragma once

#include <cstdint>

// Encodes GYRO_CONFIG_1 (bank 2, 0x01) or ACCEL_CONFIG (bank 2, 0x14); both
// share the layout [5:3] DLPFCFG, [2:1] FS_SEL, [0] FCHOICE. FCHOICE=1
// enables the DLPF and the SMPLRT_DIV dividers.
inline uint8_t filterConfigValue(uint8_t dlpfcfg, uint8_t fsSel, bool fchoice) {
    return static_cast<uint8_t>(((dlpfcfg & 0x07) << 3) |
                                ((fsSel & 0x03) << 1) |
                                (fchoice ? 1 : 0));
}

// ACCEL_SMPLRT_DIV is 12 bits split across ACCEL_SMPLRT_DIV_1 (bank 2,
// 0x10, bits [11:8]) and ACCEL_SMPLRT_DIV_2 (bank 2, 0x11, bits [7:0]).
inline uint8_t accelSmplrtDivHigh(uint16_t div) {
    return static_cast<uint8_t>((div >> 8) & 0x0F);
}

inline uint8_t accelSmplrtDivLow(uint16_t div) {
    return static_cast<uint8_t>(div & 0xFF);
}
