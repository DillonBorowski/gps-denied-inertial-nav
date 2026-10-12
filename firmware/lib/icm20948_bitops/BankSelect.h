#pragma once

#include <cstdint>

// Encodes a USER_BANK (0-3) into the bit pattern expected by REG_BANK_SEL
// (0x7F) bits [5:4], per docs/icm20948-register-map.md.
inline uint8_t bankSelectValue(uint8_t bank) {
    return static_cast<uint8_t>((bank & 0x03) << 4);
}
