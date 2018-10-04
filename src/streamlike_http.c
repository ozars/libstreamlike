#include "streamlike_http.h"
#include "streamlike_debug.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>

#include <assert.h>
#define SL_HTTP_ASSERT(...) assert(__VA_ARGS__)

static pthread_mutex_t curl_global_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int curl_global_init_done = 0;

typedef struct sl_http_s
{
    off_t http_off;
    CURL *curl;
    CURLM *curlm;

    size_t curlbuf_off;
    size_t outbuf_off;
    size_t outbuf_size;
    void *outbuf;
    enum {
        SL_HTTP_READY,
        SL_HTTP_WORKING,
        SL_HTTP_PAUSED,
        SL_HTTP_ABORTED
    } state;
} sl_http_t;

static
size_t sl_http_write_cb_(void *curlbuf, size_t ignore_this, size_t curlbuf_size,
                         void *context)
{
    sl_http_t *http = context;
    size_t outbuf_avail = http->outbuf_size - http->outbuf_off;
    size_t curlbuf_avail = curlbuf_size - http->curlbuf_off;
    void *inp = http->outbuf + http->outbuf_off;
    void *outp = curlbuf + http->curlbuf_off;

    SL_LOG("curlbuf[%p, %zu, %zu], outbuf[%p, %zu, %zu]", curlbuf,
           http->curlbuf_off, curlbuf_size, http->outbuf, http->outbuf_off,
           http->outbuf_size);
    if (http->state == SL_HTTP_ABORTED) {
        assert(-1 != CURL_WRITEFUNC_PAUSE);
        SL_LOG("Aborted.");
        return -1;
    }

    if (curlbuf_avail < outbuf_avail) {
        memcpy(inp, outp, curlbuf_avail);
        http->outbuf_off += curlbuf_avail;
        http->curlbuf_off = 0;
        http->http_off += curlbuf_avail;
        SL_LOG("Partial read.");
        return curlbuf_size;
    }
    memcpy(inp, outp, outbuf_avail);
    http->outbuf_off += outbuf_avail;
    http->curlbuf_off += outbuf_avail;
    http->http_off += outbuf_avail;
    http->state = SL_HTTP_PAUSED;
    SL_LOG("Paused.");
    return CURL_WRITEFUNC_PAUSE;
}

static
size_t sl_http_header_cb_(void *curlbuf, size_t ignore_this, size_t curlbuf_size,
                         void *context)
{
    /* TODO: Handle content-length and range stuff. */
    return 0;
}

static
void sl_cancel_transfer_(streamlike_t *stream)
{
    int count = 1;
    sl_http_t *http = stream->context;
    CURLcode ret;
    if (http->state == SL_HTTP_READY) {
        return;
    }
    if (http->state == SL_HTTP_PAUSED) {
        ret = curl_easy_pause(http->curl, CURLPAUSE_CONT);
        assert(ret == CURLM_OK);
    }
    http->state = SL_HTTP_ABORTED;

    ret = curl_multi_wait(http->curlm, NULL, 0, 0, NULL);
    assert(ret != CURLM_OK);

    ret = curl_multi_perform(http->curlm, &count);
    assert(ret == CURLM_OK && !count);
}

void sl_http_library_init()
{
    pthread_mutex_lock(&curl_global_init_mutex);
    if (!curl_global_init_done) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_global_init_done = 1;
        SL_LOG("Initialized library.");
    }
    pthread_mutex_unlock(&curl_global_init_mutex);
}

void sl_http_library_cleanup()
{
    pthread_mutex_lock(&curl_global_init_mutex);
    if (curl_global_init_done) {
        curl_global_cleanup();
        curl_global_init_done = 0;
        SL_LOG("Cleaned up library.");
    }
    pthread_mutex_unlock(&curl_global_init_mutex);
}

int sl_http_open(streamlike_t *stream, const char *uri)
{
    sl_http_t *http;

    if (!stream) {
        return 2;
    }
    http = malloc(sizeof(sl_http_t));
    if (!http) {
        return 2;
    }
    http->curl = curl_easy_init();
    if (!http->curl) {
        free(http);
        return 2;
    }
    http->curlm = curl_multi_init();
    if (!http->curlm) {
        curl_easy_cleanup(http->curl);
        free(http);
        return 2;
    }
    curl_multi_add_handle(http->curlm, http->curl);

    curl_easy_setopt(http->curl, CURLOPT_URL, uri);
    curl_easy_setopt(http->curl, CURLOPT_WRITEFUNCTION, sl_http_write_cb_);
    curl_easy_setopt(http->curl, CURLOPT_WRITEDATA, http);
    http->http_off = 0;
    http->state = SL_HTTP_READY;

    stream->context = http;
    stream->read    = sl_http_read_cb;
    stream->input   = NULL;
    stream->write   = NULL;
    stream->flush   = NULL;
    stream->seek    = sl_http_seek_cb;
    stream->tell    = sl_http_tell_cb;
    stream->eof     = sl_http_eof_cb;
    stream->error   = sl_http_error_cb;
    stream->length  = sl_http_length_cb;

    stream->seekable     = sl_http_seekable_cb;
    stream->ckp_count    = NULL;
    stream->ckp          = NULL;
    stream->ckp_offset   = NULL;
    stream->ckp_metadata = NULL;

    return 0;
}

streamlike_t* sl_http_create(const char *uri)
{
    streamlike_t *stream;
    stream = malloc(sizeof(streamlike_t));
    if (!stream) {
        return NULL;
    }
    if (sl_http_open(stream, uri) != 0) {
        free(stream);
        return NULL;
    }
    return stream;
}

int sl_http_close(streamlike_t *stream)
{
    sl_http_t *http;
    if (!stream) {
        return 2;
    }
    http = stream->context;
    if (!http) {
        return 2;
    }
    if (!http->curl) {
        return 2;
    }
    if (!http->curlm) {
        return 2;
    }
    curl_easy_cleanup(http->curl);
    curl_multi_cleanup(http->curlm);
    free(http);
    return 0;
}

int sl_http_destroy(streamlike_t *stream)
{
    int ret;
    ret = sl_http_close(stream);
    if (ret != 0) {
        return ret;
    }
    free(stream);
    return 0;
}

size_t sl_http_read_cb(void *context, void *buffer, size_t len)
{
    sl_http_t *http = context;
    http->outbuf = buffer;
    http->outbuf_off = 0;
    http->outbuf_size = len;
    int count = 1;
    if (http->state == SL_HTTP_PAUSED) {
        curl_easy_pause(http->curl, CURLPAUSE_CONT);
    }
    while (http->outbuf_off < http->outbuf_size) {
        CURLMcode result;
        if (curl_multi_wait(http->curlm, NULL, 0, 0, NULL) != CURLM_OK) {
            return http->outbuf_off;
        }
        if (curl_multi_perform(http->curlm, &count) != CURLM_OK || !count) {
            if (!count) {
                http->state = SL_HTTP_READY;
            }
            SL_LOG("Short read %zu.", http->outbuf_off);
            return http->outbuf_off;
        } else {
            if (http->state == SL_HTTP_READY) {
                http->state = SL_HTTP_WORKING;
            }
        }
    }
    /* if (http->state == SL_HTTP_WORKING) { */
    /*     curl_easy_pause(http->curl, CURLPAUSE_ALL); */
    /* } */
    return http->outbuf_off;
}

int sl_http_seek_cb(void *context, off_t offset, int whence)
{
}

off_t sl_http_tell_cb(void *context)
{
}

int sl_http_eof_cb(void *context)
{
}

int sl_http_error_cb(void *context)
{
}

off_t sl_http_length_cb(void *context)
{
}

sl_seekable_t sl_http_seekable_cb(void *context)
{
}
