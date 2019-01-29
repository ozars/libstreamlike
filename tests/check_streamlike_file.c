#include <stdlib.h>
#include <string.h>
#include <check.h>

#include "streamlike/file.h"
#include "streamlike/test.h"

#define TEMP_FILE_NAME "test.tmp"

streamlike_t *stream;
FILE *tmpf;

void verify_stream_integrity(streamlike_t *stream)
{
    ck_assert(stream != NULL);

    ck_assert(stream->context != NULL);
    ck_assert(stream->read    == sl_fread_cb);
    ck_assert(stream->input   == NULL);
    ck_assert(stream->write   == sl_fwrite_cb);
    ck_assert(stream->flush   == sl_fflush_cb);
    ck_assert(stream->seek    == sl_fseek_cb);
    ck_assert(stream->tell    == sl_ftell_cb);
    ck_assert(stream->eof     == sl_feof_cb);
    ck_assert(stream->error   == sl_ferror_cb);
    ck_assert(stream->length  == sl_flength_cb);

    ck_assert(stream->seekable     == sl_fseekable_cb);
    ck_assert(stream->ckp_count    == NULL);
    ck_assert(stream->ckp          == NULL);
    ck_assert(stream->ckp_offset   == NULL);
    ck_assert(stream->ckp_metadata == NULL);
}

START_TEST(test_create_destroy)
{
    streamlike_t* stream = sl_fopen(TEMP_FILE_NAME, "wb");
    verify_stream_integrity(stream);
    ck_assert(sl_fclose(stream) == 0);
    ck_assert(remove(TEMP_FILE_NAME) == 0);
}
END_TEST

START_TEST(test_create_destroy2)
{
    FILE* fp;
    streamlike_t* stream;

    fp = fopen(TEMP_FILE_NAME, "wb");
    ck_assert(fp != NULL);

    stream = sl_fopen2(fp);
    verify_stream_integrity(stream);

    ck_assert(sl_fclose(stream) == 0);
    ck_assert(remove(TEMP_FILE_NAME) == 0);
}
END_TEST

START_TEST(test_create_destroy_failures)
{
    /* TODO: Test failures. Requires debug build and LD_PRELOAD voodoo. */
}
END_TEST

void setup_stream()
{
    ck_assert(stream == NULL);

    tmpf = tmpfile();
    ck_assert(tmpf);

    stream = sl_fopen2(tmpf);
    ck_assert(stream);
}

void teardown_stream()
{
    ck_assert(sl_fclose(stream) == 0);
    stream = NULL;
}

START_TEST(test_read_write_seek_length)
{
    const char data[] = "\0Test data \0to write\n\r\b\t.\0";
    char buf[sizeof(data)];

    ck_assert(sl_tell(stream) == 0);
    ck_assert(sl_write(stream, data, sizeof(data)) == sizeof(data));
    ck_assert(sl_tell(stream) == sizeof(data));
    ck_assert(sl_flush(stream) == 0);

    ck_assert(sl_length(stream) == sizeof(data));

    ck_assert(sl_seek(stream, 0, SL_SEEK_SET) == 0);
    ck_assert(sl_read(stream, buf, sizeof(data)) == sizeof(data));
    ck_assert(memcmp(data, buf, sizeof(data)) == 0);
    ck_assert(sl_tell(stream) == sizeof(data));

    ck_assert(sl_length(stream) == sizeof(data));

    ck_assert(!sl_eof(stream));
    ck_assert(sl_read(stream, buf, sizeof(data)) == 0);
    ck_assert(sl_eof(stream));

    ck_assert(sl_seek(stream, 0, SL_SEEK_SET) == 0);
    ck_assert(!sl_eof(stream));

    ck_assert(sl_seekable(stream) == SL_SEEKING_SUPPORTED);
}
END_TEST

Suite* streamlike_file_suite()
{
    Suite *s;
    TCase *tc1;
    TCase *tc2;

    s = suite_create("Streamlike File");

    tc1 = tcase_create("Create/Destroy");

    tcase_add_test(tc1, test_create_destroy);
    tcase_add_test(tc1, test_create_destroy2);
    tcase_add_test(tc1, test_create_destroy_failures);
    suite_add_tcase(s, tc1);

    tc2 = tcase_create("Access/Modify");
    tcase_add_checked_fixture(tc2, setup_stream, teardown_stream);

    tcase_add_test(tc2, test_read_write_seek_length);
    suite_add_tcase(s, tc2);

    return s;
}

const char simple_data[] = "Test\0data \0 to read%\n\r\b\t.<>/\\`'\"\0";

SL_TEST_TO_CK_TEST(
    test_simple_read, sl_test_read_exact,
    SL_ARGS(stream, simple_data, sizeof(simple_data), sizeof(simple_data))
);
SL_TEST_TO_CK_TEST(
    test_simple_read_eof, sl_test_read_until_eof,
    SL_ARGS(stream, simple_data, sizeof(simple_data), sizeof(simple_data))
);

void setup_simple_stream()
{
    setup_stream();
    ck_assert_int_eq(fwrite(simple_data, sizeof(simple_data), 1, tmpf), 1);
    rewind(tmpf);
}

void teardown_simple_stream()
{
    teardown_stream();
}

Suite* streamlike_file_sl_suite()
{
    Suite *s;
    TCase *tc;

    s = suite_create("SL File");
    tc = tcase_create("Simple Read");
    tcase_add_test(tc, test_simple_read);
    tcase_add_test(tc, test_simple_read_eof);
    tcase_add_checked_fixture(tc, setup_simple_stream, teardown_simple_stream);
    suite_add_tcase(s, tc);
    return s;
}

int main(int argc, char **argv)
{
    SRunner *sr;
    int num_failed;

    sr = srunner_create(streamlike_file_suite());
    srunner_add_suite(sr, streamlike_file_sl_suite());

    srunner_run_all(sr, CK_ENV);

    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
