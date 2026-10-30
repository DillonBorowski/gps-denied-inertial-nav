#include <unity.h>
#include "GyroBias.h"

void test_bias_matches_settled_fit() {
    // data/calibration/gyro-bias.json, cold-start capture after 5 min
    TEST_ASSERT_EQUAL_FLOAT(22.61f, GYRO_BIAS_COUNTS[0]);
    TEST_ASSERT_EQUAL_FLOAT(67.45f, GYRO_BIAS_COUNTS[1]);
    TEST_ASSERT_EQUAL_FLOAT(2.05f, GYRO_BIAS_COUNTS[2]);
}

void test_removes_bias_from_raw_counts() {
    int16_t raw[3] = {23, 67, 2};
    float out[3];
    removeGyroBias(raw, out);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.39f, out[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -0.45f, out[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -0.05f, out[2]);
}

void test_handles_full_scale_negative() {
    int16_t raw[3] = {-32768, 0, 0};
    float out[3];
    removeGyroBias(raw, out);
    TEST_ASSERT_FLOAT_WITHIN(1e-2f, -32790.61f, out[0]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_bias_matches_settled_fit);
    RUN_TEST(test_removes_bias_from_raw_counts);
    RUN_TEST(test_handles_full_scale_negative);
    return UNITY_END();
}
