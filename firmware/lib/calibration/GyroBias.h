#pragma once
#include <cstdint>

// Gyro bias in raw counts (+/-250 dps, 131 LSB/dps), fit offline by
// analysis/fit_gyro_bias.py. Source: data/calibration/gyro-bias.json
// (cold-start capture 2026-10-24, first 5 min skipped, die temp ~22.2 C).
// Wait >= 5 min after power-on before relying on it.
// Logs stay raw; this is for the on-device filter.
constexpr float GYRO_BIAS_COUNTS[3] = {22.61f, 67.45f, 2.05f};

inline void removeGyroBias(const int16_t raw[3], float out[3]) {
    for (int i = 0; i < 3; i++) {
        out[i] = static_cast<float>(raw[i]) - GYRO_BIAS_COUNTS[i];
    }
}
