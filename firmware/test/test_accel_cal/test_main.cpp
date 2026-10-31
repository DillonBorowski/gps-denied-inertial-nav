#include <unity.h>
#include "AccelCal.h"

void test_values_match_six_position_fit() {
    // data/calibration/accel-6pos.json, 2026-10-30
    TEST_ASSERT_EQUAL_FLOAT(71.4f, ACCEL_OFFSET_COUNTS[0]);
    TEST_ASSERT_EQUAL_FLOAT(-243.0f, ACCEL_OFFSET_COUNTS[1]);
    TEST_ASSERT_EQUAL_FLOAT(-122.8f, ACCEL_OFFSET_COUNTS[2]);
    TEST_ASSERT_EQUAL_FLOAT(16416.0f, ACCEL_SCALE_COUNTS_PER_G[0]);
    TEST_ASSERT_EQUAL_FLOAT(16421.6f, ACCEL_SCALE_COUNTS_PER_G[1]);
    TEST_ASSERT_EQUAL_FLOAT(16451.5f, ACCEL_SCALE_COUNTS_PER_G[2]);
}

void test_offset_reads_zero_g() {
    int16_t raw[3] = {71, -243, -123};
    float g[3];
    correctAccel(raw, g);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, g[i]);
    }
}

void test_offset_plus_scale_reads_one_g() {
    // offset + one scale step on z is exactly +1 g
    int16_t raw[3] = {0, 0, static_cast<int16_t>(-123 + 16452)};
    float g[3];
    correctAccel(raw, g);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, g[2]);
}

void test_down_reads_minus_one_g() {
    int16_t raw[3] = {static_cast<int16_t>(71 - 16416), 0, 0};
    float g[3];
    correctAccel(raw, g);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -1.0f, g[0]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_values_match_six_position_fit);
    RUN_TEST(test_offset_reads_zero_g);
    RUN_TEST(test_offset_plus_scale_reads_one_g);
    RUN_TEST(test_down_reads_minus_one_g);
    return UNITY_END();
}
