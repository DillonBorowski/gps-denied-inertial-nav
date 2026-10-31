#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "MagBits.h"

// One IMU sample. seq is the timer tick, so gaps mean missed samples.
struct Sample {
    uint32_t seq;
    uint64_t tUs;                     // timer ISR time
    int16_t raw[SAMPLE_RAW_WORDS];    // ax ay az gx gy gz temp mx my mz
    uint16_t magStatus;               // AK09916 ST2 << 8 | ST1
};

// Worst case: 10 + 20 + 10*6 + 5 chars, 12 commas, newline, null = 109.
constexpr size_t SAMPLE_CSV_MAX_LEN = 128;

constexpr const char *SAMPLE_CSV_HEADER =
    "seq,t_us,ax,ay,az,gx,gy,gz,temp,mx,my,mz,mstat";

inline int formatSampleCsv(const Sample &s, char *buf, size_t len) {
    return snprintf(buf, len, "%lu,%llu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u\n",
                    static_cast<unsigned long>(s.seq),
                    static_cast<unsigned long long>(s.tUs),
                    s.raw[0], s.raw[1], s.raw[2],
                    s.raw[3], s.raw[4], s.raw[5], s.raw[6],
                    s.raw[7], s.raw[8], s.raw[9],
                    static_cast<unsigned>(s.magStatus));
}
