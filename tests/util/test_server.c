#include "test_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <check.h>

#include <arpa/inet.h>
#include <microhttpd.h>

struct test_server_s
{
    struct MHD_Daemon *httpd;
    uint16_t port;
    char address[128];
    const char *content;
    size_t content_len;
};

typedef struct test_server_ctx_s
{
    test_server_t *server;
    enum {
        TEST_SERVER_CTX_NORMAL,
        TEST_SERVER_CTX_PARTIAL,
        TEST_SERVER_CTX_BAD_HEADERS,
        TEST_SERVER_CTX_RANGE_NOT_SATISFIED,
        TEST_SERVER_CTX_NOT_SUPPORTED
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
        ctx->status = TEST_SERVER_CTX_BAD_HEADERS;
    } else if (sscanf(value += offset, "%zu-%n", &ctx->range_start, &offset)
                   != 1 || offset == 0) {
        ctx->status = TEST_SERVER_CTX_BAD_HEADERS;
    } else if (value[offset] == '\0') {
        ctx->status = TEST_SERVER_CTX_PARTIAL;
        ctx->range_end = ctx->server->content_len - 1;
    } else if (value[offset] == ',') {
        ctx->status = TEST_SERVER_CTX_NOT_SUPPORTED;
    } else if (sscanf(value += offset, "%zu%n", &ctx->range_end, &offset)
                   != 1) {
        ctx->status = TEST_SERVER_CTX_BAD_HEADERS;
    } else if (value[offset] == ',') {
        ctx->status = TEST_SERVER_CTX_NOT_SUPPORTED;
    } else if (value[offset] == '\0') {
        ctx->status = TEST_SERVER_CTX_PARTIAL;
        if (ctx->range_end > ctx->server->content_len) {
            ctx->range_end = ctx->server->content_len - 1;
        }
    } else {
        ctx->status = TEST_SERVER_CTX_BAD_HEADERS;
    }

    if (ctx->range_start > ctx->range_end
            || ctx->range_start >= ctx->server->content_len) {
        ctx->status = TEST_SERVER_CTX_RANGE_NOT_SATISFIED;
    }
    return MHD_NO;
}

int test_server_handler(void *cls, struct MHD_Connection *connection,
                        const char *url, const char *method,
                        const char *version, const char *upload_data,
                        size_t *upload_data_size, void **con_cls)
{
    struct MHD_Response *response = NULL;
    test_server_t *test_server = cls;
    test_server_ctx_t ctx = { test_server, TEST_SERVER_CTX_NORMAL, 0, 0 };
    int ret;
    int status;
    char buf[128];
    size_t range_len;
    MHD_get_connection_values(connection, MHD_HEADER_KIND,
                              test_server_get_range, &ctx);
    switch (ctx.status)
    {
        case TEST_SERVER_CTX_NORMAL:
            response = MHD_create_response_from_buffer(
                           test_server->content_len,
                           (void*)test_server->content,
                           MHD_RESPMEM_PERSISTENT);
            status = MHD_HTTP_OK;
            ret = MHD_add_response_header(response, "Accept-Ranges", "bytes");
            if (ret == MHD_NO) {
                status = MHD_HTTP_INTERNAL_SERVER_ERROR;
                goto internal_error;
            }
            ret = MHD_queue_response(connection, status, response);
            break;

        case TEST_SERVER_CTX_PARTIAL:
            range_len = ctx.range_end - ctx.range_start + 1;
            response = MHD_create_response_from_buffer(
                           range_len,
                           (void*)test_server->content + ctx.range_start,
                           MHD_RESPMEM_PERSISTENT);
            snprintf(buf, sizeof(buf), "bytes %zu-%zu/%zu",
                     ctx.range_start, ctx.range_end,
                     (size_t)test_server->content_len);
            ret = MHD_add_response_header(response, "Content-Range", buf);
            if (ret == MHD_NO) {
                status = MHD_HTTP_INTERNAL_SERVER_ERROR;
                goto internal_error;
            }
            status = 206;
            ret = MHD_queue_response(connection, status, response);
            break;

        case TEST_SERVER_CTX_RANGE_NOT_SATISFIED:
            /* TODO: 416 should include Content Range entity. It doesn't. */
            status = 416;
            goto internal_error;

        case TEST_SERVER_CTX_BAD_HEADERS:
            status = 400;
            goto internal_error;

        case TEST_SERVER_CTX_NOT_SUPPORTED:
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

test_server_t* test_server_run(const char *content, size_t content_len)
{
    const union MHD_DaemonInfo *httpd_info;
    struct sockaddr_in addr_in = {
        AF_INET,                /* family */
        0,                      /* port */
        htonl(INADDR_LOOPBACK), /* ip */
        0                       /* zeros */
    };

    test_server_t *test_server = malloc(sizeof(test_server_t));
    if (test_server == NULL) {
        return NULL;
    }
    test_server->content     = content;
    test_server->content_len = content_len;

    test_server->httpd = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, 0,
                                          NULL, NULL,
                                          &test_server_handler, test_server,
                                          MHD_OPTION_SOCK_ADDR, &addr_in,
                                          MHD_OPTION_END);
    if (test_server->httpd == NULL) {
        free(test_server);
        return NULL;
    }
    httpd_info = MHD_get_daemon_info(test_server->httpd,
                                     MHD_DAEMON_INFO_BIND_PORT);

    if (httpd_info == NULL) {
        test_server_stop(test_server);
        return NULL;
    }
    test_server->port = httpd_info->port;

    snprintf(test_server->address, sizeof(test_server->address),
             "http://%s:%d/",
             inet_ntoa((struct in_addr) { htonl(INADDR_LOOPBACK) }),
             test_server->port);

    return test_server;
}

void test_server_stop(test_server_t *test_server)
{
    if (test_server != NULL) {
        MHD_stop_daemon(test_server->httpd);
    }
    free(test_server);
}

uint16_t test_server_port(test_server_t *test_server)
{
    if (test_server == NULL) {
        return 0;
    }
    return test_server->port;
}

const char* test_server_address(test_server_t *test_server)
{
    if (test_server == NULL) {
        return NULL;
    }
    return test_server->address;
}
