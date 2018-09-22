#include <stdlib.h>
#include <check.h>

#include "streamlike_file.h"

typedef Suite* (*SuiteCreator)();

void verify_stream_integrity(streamlike_t *stream)
{
    ck_assert(stream != NULL);

    ck_assert(stream->context != NULL);
    ck_assert(stream->read    == sl_fread);
    ck_assert(stream->input   == NULL);
    ck_assert(stream->write   == sl_fwrite);
    ck_assert(stream->seek    == sl_fseek);
    ck_assert(stream->tell    == sl_ftell);
    ck_assert(stream->eof     == sl_feof);
    ck_assert(stream->error   == sl_ferror);
    ck_assert(stream->length  == sl_flength);

    ck_assert(stream->seekable     == sl_fseekable);
    ck_assert(stream->ckp_count    == NULL);
    ck_assert(stream->ckp          == NULL);
    ck_assert(stream->ckp_offset   == NULL);
    ck_assert(stream->ckp_metadata == NULL);
}

START_TEST(test_create_destroy)
{
    streamlike_t* stream = sl_fopen("test.tmp", "wb");
    verify_stream_integrity(stream);
    ck_assert(sl_fclose(stream) == 0);
}
END_TEST

START_TEST(test_create_destroy2)
{
    FILE* fp;
    streamlike_t* stream;

    fp = fopen("test.tmp", "wb");
    ck_assert(fp != NULL);

    stream = sl_fopen2(fp);
    verify_stream_integrity(stream);

    ck_assert(sl_fclose(stream) == 0);
}
END_TEST

START_TEST(test_create_destroy_failures)
{
    /* TODO: Test failures. Requires debug build and LD_PRELOAD voodoo. */
}
END_TEST

Suite* streamlike_file_suite()
{
    Suite *s;
    TCase *tc1;

    s = suite_create("Streamlike File");

    tc1 = tcase_create("Create/Destroy");

    tcase_add_test(tc1, test_create_destroy);
    tcase_add_test(tc1, test_create_destroy2);
    tcase_add_test(tc1, test_create_destroy_failures);
    suite_add_tcase(s, tc1);

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
