#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>

#include "streamlike/buffer.h"
#include "streamlike/file.h"
#include "util/test_server.h"
#include "util/util.h"

#define TEST_DATA_LENGTH      (1024*1024)
#define TEST_DATA_RANDOM_SEED (0)
#define TEST_BUFFER_SIZE (1021)
#define TEST_BUFFER_STEP_SIZE (509)

const char *temp_file_path;
streamlike_t *file_stream;
streamlike_t *http_stream;
streamlike_t *buffer_stream;
test_server_t *test_server;
char test_data[TEST_DATA_LENGTH];

START_TEST(test_stream_integrity)
{
    streamlike_t *stream = buffer_stream;

    ck_assert_ptr_nonnull(stream);

    ck_assert_ptr_nonnull(stream->context);
    ck_assert_ptr_eq(stream->read, sl_buffer_read_cb);
    ck_assert_ptr_eq(stream->input, sl_buffer_input_cb);
    ck_assert_ptr_eq(stream->seek, sl_buffer_seek_cb);
    ck_assert_ptr_eq(stream->tell, sl_buffer_tell_cb);
    ck_assert_ptr_eq(stream->eof, sl_buffer_eof_cb);
    ck_assert_ptr_eq(stream->error, sl_buffer_error_cb);
    ck_assert_ptr_eq(stream->length, sl_buffer_length_cb);

    ck_assert_ptr_eq(stream->seekable, sl_buffer_seekable_cb);
    ck_assert_ptr_eq(stream->ckp_count, sl_buffer_ckp_count_cb);
    ck_assert_ptr_eq(stream->ckp, sl_buffer_ckp_cb);
    ck_assert_ptr_eq(stream->ckp_offset, sl_buffer_ckp_offset_cb);
    ck_assert_ptr_eq(stream->ckp_metadata, sl_buffer_ckp_metadata_cb);
}
END_TEST

void setup_file()
{
    ck_assert_ptr_nonnull(temp_file_path);

    file_stream = sl_fopen(temp_file_path, "rb");
    ck_assert_ptr_nonnull(file_stream);

    buffer_stream = sl_buffer_create2(file_stream, TEST_BUFFER_SIZE,
                                      TEST_BUFFER_STEP_SIZE);
}

void teardown_file()
{
    sl_fclose(file_stream);
    file_stream = NULL;

    sl_buffer_destroy(buffer_stream);
}

START_TEST(test_file_content_verification)
{
    char *buffer = malloc(TEST_DATA_LENGTH);

    ck_assert_ptr_nonnull(buffer);
    ck_assert_int_eq(sl_read(file_stream, buffer, TEST_DATA_LENGTH),
                     TEST_DATA_LENGTH);
    ck_assert_mem_eq(buffer, test_data, TEST_DATA_LENGTH);
}
END_TEST

START_TEST(test_file_read_whole)
{
    char *buffer = malloc(TEST_DATA_LENGTH);

    ck_assert_ptr_nonnull(buffer);

    ck_assert_int_eq(sl_buffer_threaded_fill_buffer(buffer_stream), 0);

    ck_assert_int_eq(sl_tell(buffer_stream), 0);
    ck_assert_int_eq(sl_read(buffer_stream, buffer, TEST_DATA_LENGTH),
                     TEST_DATA_LENGTH);

    ck_assert_mem_eq(buffer, test_data, TEST_DATA_LENGTH);

    ck_assert_int_eq(sl_read(buffer_stream, buffer, TEST_DATA_LENGTH), 0);
    ck_assert_int_eq(sl_eof(buffer_stream), 1);
    ck_assert_int_eq(sl_tell(buffer_stream), TEST_DATA_LENGTH);
}
END_TEST

START_TEST(test_file_read_chunks)
{
    const size_t chunk_len = TEST_DATA_LENGTH / 1024;
    size_t read = 0;
    char *buffer = malloc(chunk_len);

    ck_assert_ptr_nonnull(buffer);
    ck_assert_int_eq(sl_buffer_threaded_fill_buffer(buffer_stream), 0);

    while (read < TEST_DATA_LENGTH) {
        size_t last_read;
        ck_assert_int_eq(sl_tell(buffer_stream), read);

        last_read = sl_read(buffer_stream, buffer, chunk_len);

        ck_assert_msg(last_read == chunk_len
                        || read + last_read == TEST_DATA_LENGTH,
                      "Reading the chunk of size %zd failed at offset %zd."
                      " Last read: %zd.", chunk_len, read, last_read);

        ck_assert_mem_eq(buffer, test_data + read, last_read);

        ck_assert_int_eq(sl_tell(buffer_stream), read + last_read);
        read += last_read;
    }
}
END_TEST

START_TEST(test_file_read_uneven_chunks)
{
    const size_t chunk_len = TEST_DATA_LENGTH / 1023;
    size_t read = 0;
    char *buffer = malloc(chunk_len);

    ck_assert_ptr_nonnull(buffer);
    ck_assert_int_eq(sl_buffer_threaded_fill_buffer(buffer_stream), 0);

    while (read < TEST_DATA_LENGTH) {
        size_t last_read;
        ck_assert_int_eq(sl_tell(buffer_stream), read);

        last_read = sl_read(buffer_stream, buffer, chunk_len);

        ck_assert_msg(last_read == chunk_len
                        || read + last_read == TEST_DATA_LENGTH,
                      "Reading the chunk of size %zd failed at offset %zd."
                      " Last read: %zd.", chunk_len, read, last_read);

        ck_assert_mem_eq(buffer, test_data + read, last_read);

        ck_assert_int_eq(sl_tell(buffer_stream), read + last_read);
        read += last_read;
    }
}
END_TEST

static
size_t seek_and_read(streamlike_t* buffer_stream, off_t off, char *buffer, size_t len)
{
    ck_assert_int_eq(sl_seek(buffer_stream, off, SL_SEEK_SET), 0);
    ck_assert_int_eq(sl_tell(buffer_stream), off);
    return sl_read(buffer_stream, buffer, len);
}

START_TEST(test_file_seek)
{
    const size_t chunk_len = TEST_DATA_LENGTH / 1024;
    char *buffer = malloc(chunk_len);

    ck_assert_ptr_nonnull(buffer);
    ck_assert_int_eq(sl_buffer_threaded_fill_buffer(buffer_stream), 0);
    ck_assert_int_eq(sl_tell(buffer_stream), 0);

    ck_assert_uint_gt(TEST_DATA_LENGTH, chunk_len);

    for (off_t off = 0; off < TEST_DATA_LENGTH - chunk_len; off += 5110)
    {
        ck_assert_uint_eq(seek_and_read(buffer_stream, off, buffer, chunk_len),
                        off + chunk_len);
        ck_assert_mem_eq(test_data + off, buffer, chunk_len);
    }
}
END_TEST

START_TEST(test_file_seek_after_eof)
{

}
END_TEST

Suite* streamlike_buffer_suite()
{
    Suite *s;
    TCase *tc;

    s = suite_create("Streamlike Buffer");

    tc = tcase_create("File");
    tcase_add_checked_fixture(tc, setup_file, teardown_file);
    tcase_add_test(tc, test_file_content_verification);
    tcase_add_test(tc, test_file_read_whole);
    tcase_add_test(tc, test_file_read_chunks);
    tcase_add_test(tc, test_file_read_uneven_chunks);
    tcase_add_test(tc, test_file_seek);
    suite_add_tcase(s, tc);

    return s;
}

int main(int argc, char **argv)
{
    SRunner *sr;
    int num_failed;
    FILE *write_fp;

    temp_file_path = tmpnam(NULL);
    if (temp_file_path == NULL) {
        return EXIT_FAILURE;
    }

    write_fp = fopen(temp_file_path, "wb");
    if (write_fp == NULL) {
        return EXIT_FAILURE;
    }

    fill_random_data(test_data, TEST_DATA_LENGTH, TEST_DATA_RANDOM_SEED);

    if (fwrite(test_data, 1, TEST_DATA_LENGTH, write_fp) != TEST_DATA_LENGTH) {
        fclose(write_fp);
        return EXIT_FAILURE;
    }
    fclose(write_fp);

    sr = srunner_create(streamlike_buffer_suite());

    srunner_run_all(sr, CK_ENV);

    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
