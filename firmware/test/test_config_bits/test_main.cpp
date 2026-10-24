#include <unity.h>
#include "ConfigBits.h"

void test_all_zero_fields_encode_to_zero() {
    TEST_ASSERT_EQUAL_UINT8(0x00, filterConfigValue(0, 0, false));
}

void test_dlpfcfg_3_fs_0_fchoice_on() {
    TEST_ASSERT_EQUAL_UINT8(0x19, filterConfigValue(3, 0, true));
}

void test_all_fields_max() {
    TEST_ASSERT_EQUAL_UINT8(0x3F, filterConfigValue(7, 3, true));
}

void test_out_of_range_fields_are_masked() {
    // Overflow shouldn't bleed into the next field.
    TEST_ASSERT_EQUAL_UINT8(0x00, filterConfigValue(8, 4, false));
}

void test_accel_div_high_takes_bits_11_8() {
    TEST_ASSERT_EQUAL_UINT8(0x0A, accelSmplrtDivHigh(0xABC));
}

void test_accel_div_low_takes_bits_7_0() {
    TEST_ASSERT_EQUAL_UINT8(0xBC, accelSmplrtDivLow(0xABC));
}

void test_accel_div_high_ignores_bits_above_11() {
    TEST_ASSERT_EQUAL_UINT8(0x0F, accelSmplrtDivHigh(0xFFFF));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_all_zero_fields_encode_to_zero);
    RUN_TEST(test_dlpfcfg_3_fs_0_fchoice_on);
    RUN_TEST(test_all_fields_max);
    RUN_TEST(test_out_of_range_fields_are_masked);
    RUN_TEST(test_accel_div_high_takes_bits_11_8);
    RUN_TEST(test_accel_div_low_takes_bits_7_0);
    RUN_TEST(test_accel_div_high_ignores_bits_above_11);
    return UNITY_END();
}
