#pragma once
#include <cstdint>

// Accel offset (counts) and scale (counts per g) per axis, +/-2 g range.
// Fit offline by analysis/fit_accel_6pos.py from six stationary poses.
// Source: data/calibration/accel-6pos.json (2026-10-30, die temp
// 23.2-26.6 C). Corrected |g| was 1.0000-1.0005 across all six poses.
// Logs stay raw; this is for the on-device filter.
constexpr float ACCEL_OFFSET_COUNTS[3] = {71.4f, -243.0f, -122.8f};
constexpr float ACCEL_SCALE_COUNTS_PER_G[3] = {16416.0f, 16421.6f, 16451.5f};

inline void correctAccel(const int16_t raw[3], float g[3]) {
    for (int i = 0; i < 3; i++) {
        g[i] = (static_cast<float>(raw[i]) - ACCEL_OFFSET_COUNTS[i]) /
               ACCEL_SCALE_COUNTS_PER_G[i];
    }
}
