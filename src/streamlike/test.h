/**
 * \file
 * Generic test functions for testing streamlike interfaces.
 */
#ifndef STREAMLIKE_TEST_H
#define STREAMLIKE_TEST_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "streamlike.h"

#ifndef DONT_USE_LIBCHECK
#include <check.h>
#endif

#ifndef SL_TEST_MSG_MAX_SIZE
#define SL_TEST_MSG_MAX_SIZE 1024
#endif

typedef struct sl_test_result_s
{
    const char *file;
    const char *func;
    int line;
    enum {
        SL_TEST_FAIL = 'F',
        SL_TEST_OK   = 'S',
    } status;
    struct sl_test_result_s *inner;
    char *msg;
    size_t msg_len;
} sl_test_result_t;

static inline
sl_test_result_t* sl_test_create(const char *file, const char *func, int line,
                                 int status, sl_test_result_t* inner,
                                 char *fmt, ...)
{
    sl_test_result_t *result = NULL;
    va_list argptr;
    char *msg = NULL;
    int ret;
    int msg_len;

    result = malloc(sizeof(sl_test_result_t));
    if (result == NULL) {
        goto cleanup_error;
    }
    msg = malloc(SL_TEST_MSG_MAX_SIZE);
    if (msg == NULL) {
        goto cleanup_error;
    }
    ret = snprintf(msg, SL_TEST_MSG_MAX_SIZE, "%c:%s:%d: ", (char)status,
                   func, line);
    if (ret < 0) {
        goto cleanup_error;
    }
    msg_len = ret;
    if (msg_len + 1 < SL_TEST_MSG_MAX_SIZE) {
        va_start(argptr, fmt);
        ret = vsnprintf(msg + msg_len, SL_TEST_MSG_MAX_SIZE - msg_len, fmt,
                        argptr);
        if (ret < 0) {
            msg[msg_len] = '\0';
        } else {
            msg_len += ret;
        }
        va_end(argptr);
    }
    *result = (sl_test_result_t) {file, func, line, status, inner, msg,
                                  msg_len};
    return result;

cleanup_error:
    free(result);
    free(msg);
    return NULL;
}

static inline
void sl_test_destroy(sl_test_result_t* result)
{
    if(result) {
        if (result->msg) {
            free(result->msg);
        }
        if (result->inner) {
            sl_test_destroy(result->inner);
        }
    }
}

#define sl_test_result(status, inner, fmt, ...) \
    sl_test_create(__FILE__, __func__, __LINE__, status, inner, fmt, \
                   ##__VA_ARGS__)

#define sl_test_fail(fmt, ...) \
    sl_test_result(SL_TEST_FAIL, NULL, fmt, ##__VA_ARGS__)

#define sl_test_ok sl_test_result(SL_TEST_OK, NULL, "OK")

#define return_if_not_ok(expr) \
    do { \
        sl_test_result_t *result = (expr); \
        if (result->status != SL_TEST_OK) { \
            return sl_test_result(result->status, result, \
                                  "In %s", #expr); \
        } \
        sl_test_destroy(result); \
    } while(0)

static inline
char *sl_test_failure_str(sl_test_result_t *result)
{
    char *result_str;
    sl_test_result_t *tmp;
    int n;
    if (!result || result->status == SL_TEST_OK) {
        return NULL;
    }

    for (n = 0, tmp = result; tmp; n += tmp->msg_len + 2, tmp = tmp->inner);

    result_str = malloc(n);
    if (result_str == NULL) {
        return NULL;
    }

    for(n = 0, tmp = result; tmp; tmp = tmp->inner) {
        memcpy(result_str + n, tmp->msg, tmp->msg_len);
        n += tmp->msg_len;
        result_str[n++] = '\n';
        result_str[n++] = '\t';
    }
    result_str[n-2] = '\0';

    return result_str;
}

#ifndef DONT_USE_LIBCHECK
static inline
void ck_assert_sl_test_result(sl_test_result_t *result)
{
    ck_assert_ptr_nonnull(result);
    ck_assert_msg(result->status == SL_TEST_OK, sl_test_failure_str(result));
}

#define SL_ARGS(...) __VA_ARGS__
#define SL_TEST_TO_CK_TEST(test_name, test_func, test_args) \
    START_TEST(test_name) \
    { \
        ck_assert_sl_test_result(test_func(test_args)); \
    } \
    END_TEST
#endif

static inline
sl_test_result_t* sl_test_read_exact(streamlike_t *stream,
                                     const char *expected_data,
                                     size_t data_len, size_t buf_len)
{
    char *buffer;
    size_t bytes, expected_len;

    buffer = malloc(buf_len);
    if (buffer == NULL) {
        return sl_test_fail("Couldn't allocate buffer.");
    }
    while (data_len > 0) {
        fprintf(stderr, "DATA_LEN: %zd\n", data_len);
        expected_len = (data_len > buf_len ? buf_len : data_len);
        bytes = sl_read(stream, buffer, buf_len);
        if (bytes != expected_len) {
            return sl_test_fail("Read %zd bytes while expecting %zd bytes.",
                                bytes, buf_len);
        }
        if (memcmp(buffer, expected_data, expected_len)) {
            return sl_test_fail("Failed verifying data read.");
        }
        data_len      -= bytes;
        expected_data += bytes;
    }
    return sl_test_ok;
}

static inline
sl_test_result_t* sl_test_read_until_eof(streamlike_t *stream,
                                         const char *expected_data,
                                         size_t data_len, size_t buf_len)
{
    char *buffer;
    size_t bytes;
    int ret;

    buffer = malloc(buf_len);
    if (buffer == NULL) {
        return sl_test_fail("Couldn't allocate buffer.");
    }

    while (data_len >= buf_len) {
        bytes = sl_read(stream, buffer, buf_len);
        if (bytes != buf_len) {
            return sl_test_fail("Read %zd bytes while expecting %zd bytes.",
                                bytes, buf_len);
        }
        // buffer[0] = 't';
        if (memcmp(buffer, expected_data, buf_len)) {
            return sl_test_fail("Failed verifying data read.");
        }

        if (sl_eof(stream)) {
            return sl_test_fail("Unexpected end-of-file.");
        }

        ret = sl_error(stream);
        if (ret) {
            return sl_test_fail("Unexpected error (%d) on stream.", ret);
        }
        data_len      -= bytes;
        expected_data += bytes;
    }

    bytes = sl_read(stream, buffer, buf_len);
    if (bytes != data_len) {
        return sl_test_fail("Read %zd bytes while expecting %zd bytes before "
                            "end-of-file.", bytes, data_len);
    }
    if (memcmp(buffer, expected_data, data_len)) {
        return sl_test_fail("Failed verifying data read just before "
                            "end-of-file.");
    }

    ret = sl_error(stream);
    if (ret) {
        return sl_test_fail("Unexpected error (%d) on stream.", ret);
    }

    if (!sl_eof(stream)) {
        return sl_test_fail("Not found expected end-of-file.");
    }
    return sl_test_ok;
}

#endif /* STREAMLIKE_TEST_H */
