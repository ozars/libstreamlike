#include <stdlib.h>
#include <strings.h>
#include <check.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "streamlike/http.h"
#include "util/util.h"
#include "util/test_server.h"

#define TEST_DATA_LENGTH      (1024*1024)
#define TEST_DATA_RANDOM_SEED (0)

streamlike_t *stream;
test_server_t *test_server;
char test_data[TEST_DATA_LENGTH];

void setup_global()
{
    stream = sl_http_create(test_server_address(test_server));
    ck_assert(stream);
}

void teardown_global()
{
    ck_assert(sl_http_destroy(stream) == 0);
}

START_TEST(test_single_read)
{
    char buffer[TEST_DATA_LENGTH];
    size_t len;
    len = sl_read(stream, buffer, sizeof(buffer));
    ck_assert_uint_eq(len, TEST_DATA_LENGTH);
    ck_assert_mem_eq(buffer, test_data, len);
}
END_TEST

START_TEST(test_multiple_read)
{
    char buffer[512];
    size_t len;
    size_t offset = 0;
    do {
        len = sl_read(stream, buffer, sizeof(buffer));
        ck_assert_uint_gt(len, 0);
        ck_assert_uint_le(len, TEST_DATA_LENGTH);
        ck_assert_uint_le(len, sizeof(buffer));
        ck_assert_mem_eq(buffer, test_data + offset, len);
        offset += len;
    } while(offset < TEST_DATA_LENGTH);
}
END_TEST

START_TEST(test_multiple_read2)
{
    char buffer[511];
    size_t len;
    size_t offset = 0;
    do {
        len = sl_read(stream, buffer, sizeof(buffer));
        ck_assert_uint_gt(len, 0);
        ck_assert_uint_le(len, TEST_DATA_LENGTH);
        ck_assert_uint_le(len, sizeof(buffer));
        ck_assert_mem_eq(buffer, test_data + offset, len);
        offset += len;
    } while(offset < TEST_DATA_LENGTH);
}
END_TEST

START_TEST(test_read_until_eof)
{
    char buffer[512];
    size_t len;
    size_t offset = 0;
    do {
        len = sl_read(stream, buffer, sizeof(buffer));
        ck_assert_uint_le(len, TEST_DATA_LENGTH);
        ck_assert_uint_le(len, sizeof(buffer));
        ck_assert_mem_eq(buffer, test_data + offset, len);
        offset += len;
        ck_assert_uint_le(offset, TEST_DATA_LENGTH);
    } while(!sl_eof(stream));
}
END_TEST

START_TEST(test_read_until_eof2)
{
    char buffer[511];
    size_t len;
    size_t offset = 0;
    do {
        len = sl_read(stream, buffer, sizeof(buffer));
        ck_assert_uint_le(len, TEST_DATA_LENGTH);
        ck_assert_uint_le(len, sizeof(buffer));
        ck_assert_mem_eq(buffer, test_data + offset, len);
        offset += len;
        ck_assert_uint_le(offset, TEST_DATA_LENGTH);
    } while(!sl_eof(stream));
}
END_TEST

START_TEST(test_seek_and_read_until_eof)
{
    char buffer[512];
    size_t len;
    size_t offset = TEST_DATA_LENGTH / 2;
    ck_assert_int_eq(sl_seek(stream, offset, SL_SEEK_SET), 0);
    do {
        len = sl_read(stream, buffer, sizeof(buffer));
        ck_assert_uint_le(len, TEST_DATA_LENGTH);
        ck_assert_uint_le(len, sizeof(buffer));
        ck_assert_mem_eq(buffer, test_data + offset, len);
        offset += len;
        ck_assert_uint_le(offset, TEST_DATA_LENGTH);
    } while(!sl_eof(stream));
}
END_TEST

START_TEST(test_seek_and_read_until_eof2)
{
    char buffer[511];
    size_t len;
    size_t offset = TEST_DATA_LENGTH / 2;
    ck_assert_int_eq(sl_seek(stream, offset, SL_SEEK_SET), 0);
    do {
        len = sl_read(stream, buffer, sizeof(buffer));
        ck_assert_uint_le(len, TEST_DATA_LENGTH);
        ck_assert_uint_le(len, sizeof(buffer));
        ck_assert_mem_eq(buffer, test_data + offset, len);
        offset += len;
        ck_assert_uint_le(offset, TEST_DATA_LENGTH);
    } while(!sl_eof(stream));
}
END_TEST

START_TEST(test_single_seek_and_read)
{
    const size_t seek_to = TEST_DATA_LENGTH / 2;
    char buffer[seek_to];
    size_t len;

    ck_assert_int_eq(sl_seek(stream, seek_to, SL_SEEK_SET), 0);
    len = sl_read(stream, buffer, sizeof(buffer));
    ck_assert_uint_eq(len, sizeof(buffer));
    ck_assert_mem_eq(buffer, test_data + seek_to, len);
}
END_TEST

START_TEST(test_multiple_seek_and_read)
{
    char buffer[1024];
    size_t len;
    size_t expected_len;

    for (size_t seek_to = 0; seek_to < TEST_DATA_LENGTH - 1024; seek_to += 1023)
    {
        ck_assert_int_eq(sl_seek(stream, seek_to, SL_SEEK_SET), 0);
        expected_len = seek_to + sizeof(buffer) < TEST_DATA_LENGTH
                        ? sizeof(buffer)
                        : TEST_DATA_LENGTH - seek_to;
        len = sl_read(stream, buffer, sizeof(buffer));
        ck_assert_uint_eq(len, expected_len);
        ck_assert_mem_eq(buffer, test_data + seek_to, len);
    }
}
END_TEST

START_TEST(test_multiple_seek_and_read2)
{
    char buffer[1023];
    size_t len;
    size_t expected_len;

    for (size_t seek_to = 0; seek_to < TEST_DATA_LENGTH - 1024; seek_to += 1023)
    {
        ck_assert_int_eq(sl_seek(stream, seek_to, SL_SEEK_SET), 0);
        expected_len = seek_to + sizeof(buffer) < TEST_DATA_LENGTH
                        ? sizeof(buffer)
                        : TEST_DATA_LENGTH - seek_to;
        len = sl_read(stream, buffer, sizeof(buffer));
        ck_assert_uint_eq(len, expected_len);
        ck_assert_mem_eq(buffer, test_data + seek_to, len);
    }
}
END_TEST

Suite* streamlike_http_suite()
{
    Suite *s;
    TCase *tc;

    s = suite_create("Streamlike HTTP");

    tc = tcase_create("Sequential Tests");
    tcase_add_checked_fixture(tc, setup_global, teardown_global);
    tcase_add_test(tc, test_single_read);
    tcase_add_test(tc, test_multiple_read);
    tcase_add_test(tc, test_multiple_read2);
    tcase_add_test(tc, test_read_until_eof);
    tcase_add_test(tc, test_read_until_eof2);
    tcase_add_test(tc, test_seek_and_read_until_eof);
    tcase_add_test(tc, test_seek_and_read_until_eof2);
    tcase_add_test(tc, test_single_seek_and_read);
    tcase_add_test(tc, test_multiple_seek_and_read);
    tcase_add_test(tc, test_multiple_seek_and_read2);
    suite_add_tcase(s, tc);

    return s;
}

int main(int argc, char **argv)
{
    SRunner *sr;
    int num_failed;

    fill_random_data(test_data, TEST_DATA_LENGTH, TEST_DATA_RANDOM_SEED);

    test_server = test_server_run(test_data, TEST_DATA_LENGTH);
    if(test_server == NULL) {
        return -1;
    }

    sr = srunner_create(streamlike_http_suite());

    srunner_run_all(sr, CK_ENV);

    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    test_server_stop(test_server);

    return (num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
