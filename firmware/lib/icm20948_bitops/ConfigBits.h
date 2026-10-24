#pragma once

#include <cstdint>

// GYRO_CONFIG_1 / ACCEL_CONFIG: [5:3] DLPFCFG, [2:1] FS_SEL, [0] FCHOICE.
inline uint8_t filterConfigValue(uint8_t dlpfcfg, uint8_t fsSel, bool fchoice) {
    return static_cast<uint8_t>(((dlpfcfg & 0x07) << 3) |
                                ((fsSel & 0x03) << 1) |
                                (fchoice ? 1 : 0));
}

// 12-bit accel divider: DIV_1 gets bits [11:8], DIV_2 gets [7:0].
inline uint8_t accelSmplrtDivHigh(uint16_t div) {
    return static_cast<uint8_t>((div >> 8) & 0x0F);
}

inline uint8_t accelSmplrtDivLow(uint16_t div) {
    return static_cast<uint8_t>(div & 0xFF);
}
