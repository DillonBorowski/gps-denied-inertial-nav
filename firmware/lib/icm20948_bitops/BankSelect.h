#pragma once

#include <cstdint>

// REG_BANK_SEL (0x7F) value for bank 0-3, bits [5:4].
inline uint8_t bankSelectValue(uint8_t bank) {
    return static_cast<uint8_t>((bank & 0x03) << 4);
}
