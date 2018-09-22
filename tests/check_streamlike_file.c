#include <stdlib.h>
#include <string.h>
#include <check.h>

#include "streamlike_file.h"

#define TEMP_FILE_NAME "test.tmp"

streamlike_t *tmp_stream;

void verify_stream_integrity(streamlike_t *stream)
{
    ck_assert(stream != NULL);

    ck_assert(stream->context != NULL);
    ck_assert(stream->read    == sl_fread_cb);
    ck_assert(stream->input   == NULL);
    ck_assert(stream->write   == sl_fwrite_cb);
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

void setup_tmp_stream()
{
    FILE *tmpf;

    ck_assert(tmp_stream == NULL);

    tmpf = tmpfile();
    ck_assert(tmpf);

    tmp_stream = sl_fopen2(tmpf);
    ck_assert(tmp_stream);
}

void teardown_tmp_stream()
{
    ck_assert(sl_fclose(tmp_stream) == 0);
    tmp_stream = NULL;
}

START_TEST(test_read_write_seek_length)
{
    const char data[] = "\0Test data \0to write\n\r\b\t.\0";
    char buf[sizeof(data)];

    ck_assert(sl_tell(tmp_stream) == 0);
    ck_assert(sl_write(tmp_stream, data, sizeof(data)) == sizeof(data));
    ck_assert(sl_tell(tmp_stream) == sizeof(data));
    ck_assert(sl_seek(tmp_stream, 0, SL_SEEK_SET) == 0);
    ck_assert(sl_read(tmp_stream, buf, sizeof(data)) == sizeof(data));
    ck_assert(sl_tell(tmp_stream) == sizeof(data));
    ck_assert(memcmp(data, buf, sizeof(data)) == 0);
    ck_assert(sl_length(tmp_stream) == sizeof(data));
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
    tcase_add_checked_fixture(tc2, setup_tmp_stream, teardown_tmp_stream);

    tcase_add_test(tc2, test_read_write_seek_length);
    suite_add_tcase(s, tc2);

    return s;
}

int main(int argc, char **argv)
{
    SRunner *sr;
    int num_failed;

    sr = srunner_create(streamlike_file_suite());

    srunner_run_all(sr, CK_ENV);

    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
