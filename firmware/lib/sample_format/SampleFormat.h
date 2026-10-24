#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

// One timer-driven IMU sample. seq is the timer tick number, so a gap in
// seq on the host means a sample was missed or dropped.
struct Sample {
    uint32_t seq;
    uint64_t tUs;     // esp_timer time captured in the timer ISR
    int16_t raw[7];   // {ax, ay, az, gx, gy, gz, temp} raw signed counts
};

// Longest possible line: 10 + 20 + 7*6 digits/signs, 8 commas, '\n', '\0'.
constexpr size_t SAMPLE_CSV_MAX_LEN = 96;

// Column order of formatSampleCsv(), for the stream header.
constexpr const char *SAMPLE_CSV_HEADER = "seq,t_us,ax,ay,az,gx,gy,gz,temp";

// Writes "seq,t_us,ax,ay,az,gx,gy,gz,temp\n" into buf. Returns snprintf's
// result.
inline int formatSampleCsv(const Sample &s, char *buf, size_t len) {
    return snprintf(buf, len, "%lu,%llu,%d,%d,%d,%d,%d,%d,%d\n",
                    static_cast<unsigned long>(s.seq),
                    static_cast<unsigned long long>(s.tUs),
                    s.raw[0], s.raw[1], s.raw[2],
                    s.raw[3], s.raw[4], s.raw[5], s.raw[6]);
}
