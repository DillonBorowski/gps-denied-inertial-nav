#include <unity.h>
#include <cstring>
#include "SampleFormat.h"

void test_formats_csv_line_in_column_order() {
    Sample s = {7, 123456, {1, 2, 3, 4, 5, 6, 7}};
    char buf[SAMPLE_CSV_MAX_LEN];
    formatSampleCsv(s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("7,123456,1,2,3,4,5,6,7\n", buf);
}

void test_header_has_temp_column_last() {
    TEST_ASSERT_EQUAL_STRING("seq,t_us,ax,ay,az,gx,gy,gz,temp",
                             SAMPLE_CSV_HEADER);
}

void test_negative_counts_keep_sign() {
    Sample s = {0, 0, {-1, -32768, 0, 32767, -5, 0, -300}};
    char buf[SAMPLE_CSV_MAX_LEN];
    formatSampleCsv(s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0,0,-1,-32768,0,32767,-5,0,-300\n", buf);
}

void test_timestamp_beyond_32_bits_is_not_truncated() {
    // ~79 minutes of uptime in microseconds overflows uint32_t.
    Sample s = {1, 4800000000ULL, {0, 0, 0, 0, 0, 0, 0}};
    char buf[SAMPLE_CSV_MAX_LEN];
    formatSampleCsv(s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1,4800000000,0,0,0,0,0,0,0\n", buf);
}

void test_worst_case_line_fits_max_len() {
    Sample s = {UINT32_MAX, UINT64_MAX,
                {-32768, -32768, -32768, -32768, -32768, -32768, -32768}};
    char buf[SAMPLE_CSV_MAX_LEN];
    int n = formatSampleCsv(s, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_TRUE(static_cast<size_t>(n) < sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(n, strlen(buf));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_formats_csv_line_in_column_order);
    RUN_TEST(test_header_has_temp_column_last);
    RUN_TEST(test_negative_counts_keep_sign);
    RUN_TEST(test_timestamp_beyond_32_bits_is_not_truncated);
    RUN_TEST(test_worst_case_line_fits_max_len);
    return UNITY_END();
}
