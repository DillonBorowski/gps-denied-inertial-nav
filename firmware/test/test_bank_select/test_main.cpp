#include <unity.h>
#include "BankSelect.h"

void test_bank_0_encodes_to_zero() {
    TEST_ASSERT_EQUAL_UINT8(0x00, bankSelectValue(0));
}

void test_bank_1_encodes_to_bits_5_4() {
    TEST_ASSERT_EQUAL_UINT8(0x10, bankSelectValue(1));
}

void test_bank_2_encodes_to_bits_5_4() {
    TEST_ASSERT_EQUAL_UINT8(0x20, bankSelectValue(2));
}

void test_bank_3_encodes_to_bits_5_4() {
    TEST_ASSERT_EQUAL_UINT8(0x30, bankSelectValue(3));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_bank_0_encodes_to_zero);
    RUN_TEST(test_bank_1_encodes_to_bits_5_4);
    RUN_TEST(test_bank_2_encodes_to_bits_5_4);
    RUN_TEST(test_bank_3_encodes_to_bits_5_4);
    return UNITY_END();
}
