#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <check.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>

#include "streamlike/http.h"

#define TEST_SERVER_PORT 9000
#define TEST_DATA_LENGTH (1024*1024)

#define STR(s) STR_(s)
#define STR_(s) #s

streamlike_t *stream;
struct MHD_Daemon *httpd;
const size_t content_len = TEST_DATA_LENGTH;
char *test_data;

typedef struct test_server_ctx_s
{
    enum {
        TEST_SERVER_NORMAL,
        TEST_SERVER_PARTIAL,
        TEST_SERVER_BAD_HEADERS,
        TEST_SERVER_RANGE_NOT_SATISFIED,
        TEST_SERVER_NOT_SUPPORTED
    } status;
    size_t range_start;
    size_t range_end;
} test_server_ctx_t;

int test_server_get_range(void *cls, enum MHD_ValueKind kind, const char *key,
                          const char* value)
{
    test_server_ctx_t *ctx = cls;
    int offset = 0;
    if (strcasecmp(key, "range")) {
        return MHD_YES;
    }
    if (sscanf(value, "bytes=%n", &offset) != 0 || offset != 6) {
        ctx->status = TEST_SERVER_BAD_HEADERS;
    } else if (sscanf(value += offset, "%zu-%n", &ctx->range_start, &offset)
                   != 1 || offset == 0) {
        ctx->status = TEST_SERVER_BAD_HEADERS;
    } else if (value[offset] == '\0') {
        ctx->status = TEST_SERVER_PARTIAL;
        ctx->range_end = content_len - 1;
    } else if (value[offset] == ',') {
        ctx->status = TEST_SERVER_NOT_SUPPORTED;
    } else if (sscanf(value += offset, "%zu%n", &ctx->range_end, &offset)
                   != 1) {
        ctx->status = TEST_SERVER_BAD_HEADERS;
    } else if (value[offset] == ',') {
        ctx->status = TEST_SERVER_NOT_SUPPORTED;
    } else if (value[offset] == '\0') {
        ctx->status = TEST_SERVER_PARTIAL;
        if (ctx->range_end > content_len) {
            ctx->range_end = content_len - 1;
        }
    } else {
        ctx->status = TEST_SERVER_BAD_HEADERS;
    }

    if (ctx->range_start > ctx->range_end || ctx->range_start >= content_len) {
        ctx->status = TEST_SERVER_RANGE_NOT_SATISFIED;
    }
    return MHD_NO;
}

int test_server_handler(void *cls, struct MHD_Connection *connection,
                        const char *url, const char *method,
                        const char *version, const char *upload_data,
                        size_t *upload_data_size, void **con_cls)
{
    struct MHD_Response *response = NULL;
    test_server_ctx_t ctx = { TEST_SERVER_NORMAL, 0, 0 };
    int ret;
    int status;
    char buf[128];
    size_t range_len;
    MHD_get_connection_values(connection, MHD_HEADER_KIND,
                              test_server_get_range, &ctx);
    switch (ctx.status)
    {
        case TEST_SERVER_NORMAL:
            response = MHD_create_response_from_buffer(
                           TEST_DATA_LENGTH, test_data, MHD_RESPMEM_PERSISTENT);
            status = MHD_HTTP_OK;
            ret = MHD_add_response_header(response, "Accept-Ranges", "bytes");
            if (ret == MHD_NO) {
                status = MHD_HTTP_INTERNAL_SERVER_ERROR;
                goto internal_error;
            }
            ret = MHD_queue_response(connection, status, response);
            break;

        case TEST_SERVER_PARTIAL:
            range_len = ctx.range_end - ctx.range_start + 1;
            response = MHD_create_response_from_buffer(
                           range_len, test_data + ctx.range_start,
                           MHD_RESPMEM_PERSISTENT);
            snprintf(buf, sizeof(buf), "bytes %zu-%zu/%zu",
                     ctx.range_start, ctx.range_end, (size_t)content_len);
            ret = MHD_add_response_header(response, "Content-Range", buf);
            if (ret == MHD_NO) {
                status = MHD_HTTP_INTERNAL_SERVER_ERROR;
                goto internal_error;
            }
            status = 206;
            ret = MHD_queue_response(connection, status, response);
            break;

        case TEST_SERVER_RANGE_NOT_SATISFIED:
            /* TODO: 416 should include Content Range entity. It doesn't. */
            status = 416;
            goto internal_error;

        case TEST_SERVER_BAD_HEADERS:
            status = 400;
            goto internal_error;

        case TEST_SERVER_NOT_SUPPORTED:
            status = 501;
            goto internal_error;

        internal_error:
            if (response) {
                MHD_destroy_response(response);
                response = NULL;
            }
            response = MHD_create_response_from_buffer(
                           6, "Error.", MHD_RESPMEM_PERSISTENT);
            ret = MHD_queue_response(connection, status, response);

    }
    if (response) {
        MHD_destroy_response(response);
        response = NULL;
    }
    return ret;
}

int test_server_run()
{
    test_data = malloc(TEST_DATA_LENGTH);
    if (test_data == NULL) {
        return -1;
    }
    unsigned int seed = 0;
    for (int i = 0; i < TEST_DATA_LENGTH; i++)
    {
        test_data[i] = (char)('a' + (rand_r(&seed) % 26));
    }

    httpd = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, TEST_SERVER_PORT, NULL,
                             NULL, &test_server_handler, NULL, MHD_OPTION_END);
    if (httpd == NULL) {
        return 1;
    }
    return 0;
}

void test_server_stop()
{
    MHD_stop_daemon(httpd);
    free(test_data);
}

void setup_global()
{
    stream = sl_http_create("http://localhost:" STR(TEST_SERVER_PORT) "/");
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
    ck_assert_uint_eq(len, content_len);
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

    if(test_server_run() != 0) {
        return -1;
    }

    sr = srunner_create(streamlike_http_suite());

    srunner_run_all(sr, CK_ENV);

    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    test_server_stop();

    return (num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
