#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

// One IMU sample. seq is the timer tick, so gaps mean missed samples.
struct Sample {
    uint32_t seq;
    uint64_t tUs;     // timer ISR time
    int16_t raw[7];   // ax, ay, az, gx, gy, gz, temp
};

// Worst case: 10 + 20 + 7*6 chars, 8 commas, newline, null.
constexpr size_t SAMPLE_CSV_MAX_LEN = 96;

constexpr const char *SAMPLE_CSV_HEADER = "seq,t_us,ax,ay,az,gx,gy,gz,temp";

inline int formatSampleCsv(const Sample &s, char *buf, size_t len) {
    return snprintf(buf, len, "%lu,%llu,%d,%d,%d,%d,%d,%d,%d\n",
                    static_cast<unsigned long>(s.seq),
                    static_cast<unsigned long long>(s.tUs),
                    s.raw[0], s.raw[1], s.raw[2],
                    s.raw[3], s.raw[4], s.raw[5], s.raw[6]);
}
