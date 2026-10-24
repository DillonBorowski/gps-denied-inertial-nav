#pragma once

#include <cstdint>

// High/low register bytes to a signed 16-bit value.
inline int16_t combineBigEndian(uint8_t high, uint8_t low) {
    return static_cast<int16_t>(static_cast<uint16_t>(high) << 8 | low);
}
