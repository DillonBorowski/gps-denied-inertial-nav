#include <unity.h>
#include "MagBits.h"

void test_little_endian_puts_low_byte_first() {
    TEST_ASSERT_EQUAL_INT16(0x0201, combineLittleEndian(0x01, 0x02));
}

void test_little_endian_negative() {
    TEST_ASSERT_EQUAL_INT16(-2, combineLittleEndian(0xFE, 0xFF));
}

void test_periph_addr_sets_read_bit() {
    TEST_ASSERT_EQUAL_HEX8(0x8C, i2cPeriphAddrValue(0x0C, true));
    TEST_ASSERT_EQUAL_HEX8(0x0C, i2cPeriphAddrValue(0x0C, false));
}

void test_periph_addr_masks_to_7_bits() {
    TEST_ASSERT_EQUAL_HEX8(0x0C, i2cPeriphAddrValue(0x8C, false));
}

void test_periph_ctrl_enable_and_length() {
    TEST_ASSERT_EQUAL_HEX8(0x89, i2cPeriphCtrlValue(true, 9));
    TEST_ASSERT_EQUAL_HEX8(0x09, i2cPeriphCtrlValue(false, 9));
}

void test_periph_ctrl_length_is_4_bits() {
    TEST_ASSERT_EQUAL_HEX8(0x8F, i2cPeriphCtrlValue(true, 0x1F));
}

void test_decodes_sensor_block() {
    // 14 bytes accel/gyro/temp big-endian, then the AK09916 read:
    // ST1, HXL, HXH, HYL, HYH, HZL, HZH, TMPS (dummy), ST2.
    uint8_t block[SENSOR_BLOCK_LEN] = {
        0x00, 0x01, 0xFF, 0xFF, 0x40, 0x00,   // ax 1, ay -1, az 16384
        0x00, 0x16, 0x00, 0x43, 0x00, 0x02,   // gx 22, gy 67, gz 2
        0x02, 0x10,                           // temp 528
        0x01,                                 // ST1: DRDY
        0x2C, 0x01, 0x9C, 0xFF, 0x00, 0xFE,   // mx 300, my -100, mz -512
        0x00,                                 // TMPS
        0x08,                                 // ST2: HOFL
    };
    int16_t raw[SAMPLE_RAW_WORDS];
    uint16_t magStatus;
    decodeSensorBlock(block, raw, magStatus);
    const int16_t want[SAMPLE_RAW_WORDS] = {1, -1, 16384, 22, 67, 2, 528,
                                            300, -100, -512};
    TEST_ASSERT_EQUAL_INT16_ARRAY(want, raw, SAMPLE_RAW_WORDS);
    TEST_ASSERT_EQUAL_HEX16(0x0801, magStatus);  // ST2 << 8 | ST1
}

void test_block_length_is_23_bytes() {
    TEST_ASSERT_EQUAL(23, SENSOR_BLOCK_LEN);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_little_endian_puts_low_byte_first);
    RUN_TEST(test_little_endian_negative);
    RUN_TEST(test_periph_addr_sets_read_bit);
    RUN_TEST(test_periph_addr_masks_to_7_bits);
    RUN_TEST(test_periph_ctrl_enable_and_length);
    RUN_TEST(test_periph_ctrl_length_is_4_bits);
    RUN_TEST(test_decodes_sensor_block);
    RUN_TEST(test_block_length_is_23_bytes);
    return UNITY_END();
}
