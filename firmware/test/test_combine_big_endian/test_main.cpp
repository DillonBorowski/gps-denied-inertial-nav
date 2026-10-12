#include <unity.h>
#include "CombineBigEndian.h"

void test_zero_bytes_combine_to_zero() {
    TEST_ASSERT_EQUAL_INT16(0, combineBigEndian(0x00, 0x00));
}

void test_positive_value_combines_high_then_low() {
    TEST_ASSERT_EQUAL_INT16(0x0102, combineBigEndian(0x01, 0x02));
}

void test_negative_value_combines_as_twos_complement() {
    // 0xFFFF as int16_t is -1.
    TEST_ASSERT_EQUAL_INT16(-1, combineBigEndian(0xFF, 0xFF));
}

void test_max_positive_high_byte() {
    // 0x7FFF is the largest positive int16_t.
    TEST_ASSERT_EQUAL_INT16(32767, combineBigEndian(0x7F, 0xFF));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_zero_bytes_combine_to_zero);
    RUN_TEST(test_positive_value_combines_high_then_low);
    RUN_TEST(test_negative_value_combines_as_twos_complement);
    RUN_TEST(test_max_positive_high_byte);
    return UNITY_END();
}
