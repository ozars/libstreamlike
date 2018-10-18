#include "streamlike_http.h"
#include "streamlike_debug.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <curl/curl.h>

#include <assert.h>
#define SL_HTTP_ASSERT(...) assert(__VA_ARGS__)

static pthread_mutex_t curl_global_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int curl_global_init_done = 0;

typedef struct sl_http_s
{
    off_t http_off;
    off_t http_len;
    int http_status;
    CURL *curl;
    CURLM *curlm;

    size_t curlbuf_off;
    size_t outbuf_off;
    size_t outbuf_size;
    void *outbuf;
    enum {
        SL_HTTP_READY = 0,
        SL_HTTP_WORKING = 1,
        SL_HTTP_PAUSED = 2,
        SL_HTTP_ABORT_REQUESTED = 3,
        SL_HTTP_ABORTED = 4
    } state;
} sl_http_t;

static
void sl_http_set_state_(sl_http_t *http, int state)
{
    switch (state)
    {
        case SL_HTTP_READY:
            SL_LOG("Setting state to READY.");
            break;
        case SL_HTTP_WORKING:
            SL_LOG("Setting state to WORKING.");
            break;
        case SL_HTTP_PAUSED:
            SL_LOG("Setting state to PAUSED.");
            break;
        case SL_HTTP_ABORT_REQUESTED:
            SL_LOG("Setting state to ABORT_REQUESTED.");
            break;
        case SL_HTTP_ABORTED:
            SL_LOG("Setting state to ABORTED.");
            break;
        default:
            SL_LOG("Unexpected state: %d", state);
            abort();
    }
    http->state = state;
}

static
size_t sl_http_write_cb_(void *curlbuf, size_t ignore_this, size_t curlbuf_size,
                         void *context)
{
    sl_http_t *http = context;
    size_t outbuf_avail = http->outbuf_size - http->outbuf_off;
    size_t curlbuf_avail = curlbuf_size - http->curlbuf_off;
    void *inp = http->outbuf + http->outbuf_off;
    void *outp = curlbuf + http->curlbuf_off;

    if (http->state == SL_HTTP_ABORT_REQUESTED) {
        sl_http_set_state_(http, SL_HTTP_ABORTED);
        curl_multi_remove_handle(http->curlm, http->curl);
        curl_multi_add_handle(http->curlm, http->curl);
        SL_LOG("Aborted.");
        return -1;
    }

    if (curlbuf_avail < outbuf_avail) {
        SL_LOG("Partial read from curlbuf[%zu..%zu] to outbuf[%zu..%zu] of "
               "length %zu.", http->curlbuf_off, curlbuf_size - 1,
               http->outbuf_off, http->outbuf_size - 1, curlbuf_avail);
        memcpy(inp, outp, curlbuf_avail);
        http->outbuf_off += curlbuf_avail;
        http->curlbuf_off = 0;
        http->http_off += curlbuf_avail;
        return curlbuf_size;
    }
    SL_LOG("Paused after read from curlbuf[%zu..%zu] to outbuf[%zu..%zu] of "
           "length %zu.", http->curlbuf_off, curlbuf_size - 1,
           http->outbuf_off, http->outbuf_size - 1, outbuf_avail);
    memcpy(inp, outp, outbuf_avail);
    http->outbuf_off += outbuf_avail;
    http->curlbuf_off += outbuf_avail;
    http->http_off += outbuf_avail;
    sl_http_set_state_(http, SL_HTTP_PAUSED);
    return CURL_WRITEFUNC_PAUSE;
}

static
int memtrim(const char *mem, size_t size, const char **begin, const char **end)
{
    const char *temp_begin;
    const char *temp_end;
    if (begin == NULL) {
        begin = &temp_begin;
    }
    if (end == NULL) {
        end = &temp_end;
    }
    *begin = mem;
    *end   = mem + size;
    for(; *end > *begin && isspace((unsigned char)*(*end-1)); (*end)--);
    for(; *begin < *end && isspace((unsigned char)**begin); (*begin)++);

    return *end == *begin;
}

static
int memcasestartswith(const char *s1, size_t n1, const char *s2, size_t n2)
{
    return n1 >= n2 && !strncasecmp(s1, s2, n2);
}

/* TODO: This function aborts on some conditions. Should be fixed. */
static
size_t sl_http_header_cb_(void *curlbuf, size_t nitems, size_t curlbuf_size,
                          void *context)
{
    sl_http_t *http = context;

    /* Update curlbuf size. */
    curlbuf_size *= nitems;

    /* Shortcut if there is nothing. */
    if (curlbuf_size == 0) {
        return 0;
    }

    const char *line;
    const char *line_end;

    /* Trim, then return if line is all whitespaces. */
    if (memtrim(curlbuf, curlbuf_size, &line, &line_end)) {
        SL_LOG("Skipping all whitespaces header entry.");
        return curlbuf_size;
    }

    /* TODO: Is this scanf secure for a non-NULL terminated line? */
    if (sscanf(line, "HTTP/%*s %d ", &http->http_status) == 1) {
        SL_LOG("HTTP status read: %d", http->http_status);
        return curlbuf_size;
    }

    /* If there is a colon... */
    const char *colon = memchr(line, ':', line_end - line);
    if (colon) {
        SL_LOG("Parsing header line: '%.*s'", (int)(line_end - line), line);

        /* Log and return if colon is the last character. */
        if (colon + 1 == line_end) {
            SL_LOG("Skipping header entry with no value: '%.*s'",
                   (int)(line_end - line), line);
            return curlbuf_size;
        }

        /* Log and return if colon is the first character. */
        if (colon == line) {
            SL_LOG("Skipping header entry with no key: '%.*s'",
                   (int)(line_end - line), line);
            return curlbuf_size;
        }

        /* Get the trimmed value and return if it's empty... */
        const char *value;
        if (memtrim(colon + 1, line_end - colon - 1, &value, NULL)) {
            SL_LOG("Skipping header entry with empty value: '%.*s'",
                   (int)(line_end - line), line);
            return curlbuf_size;
        }

        /* Set length of the header key. */
        size_t key_len = colon - line;

        /* Headers of interests. */
        const char cnt_len[] = "Content-Length";
        const char cnt_rng[] = "Content-Range";

        if (http->http_len < 0) {
            /* If the content-range is found and status is 406 Partial Content,
             * read total length of stream. */
            if (http->http_status == 206
                    && memcasestartswith(line, key_len, cnt_rng,
                                         sizeof(cnt_rng) - 1)) {
                const char *length_part = memchr(value, '/', line_end - value);
                if (!length_part) {
                    SL_LOG("Skipping header entry as there is no length divider: "
                           "'%.*s'", (int)(line_end - line), line);
                    return curlbuf_size;
                }
                if (++length_part == line_end) {
                    SL_LOG("Skipping header entry as length divider is at the end: "
                           "'%.*s'", (int)(line_end - line), line);
                    return curlbuf_size;
                }
                http->http_len = strtoull(length_part, NULL, 10);
                SL_LOG("Set http length to %jd from content range.",
                       (intmax_t)http->http_len);

            /* ...or if content-length is found and status is 200 OK, read total
             * length of the stream. */
            } else if (http->http_status == 200 &&
                       memcasestartswith(line, key_len, cnt_len,
                                         sizeof(cnt_len) - 1)) {
                http->http_len = strtoull(value, NULL, 10);
                SL_LOG("Set http length to %jd from content length.",
                       (intmax_t)http->http_len);
            }

        }
    }
    return curlbuf_size;
}

static
void sl_cancel_transfer_(sl_http_t *http)
{
    int count = 1;
    CURLcode ret;
    SL_LOG("Trying to cancel...");
    if (http->state == SL_HTTP_READY) {
        SL_LOG("No need to cancel.");
        return;
    }
    if (http->state == SL_HTTP_PAUSED) {
        SL_LOG("Unpausing for cancellation...");
        sl_http_set_state_(http, SL_HTTP_ABORT_REQUESTED);
        ret = curl_easy_pause(http->curl, CURLPAUSE_CONT);
        assert(ret == CURLE_WRITE_ERROR);
    } else {
        sl_http_set_state_(http, SL_HTTP_ABORT_REQUESTED);
    }

    if (http->state != SL_HTTP_ABORTED) {
        SL_LOG("Concluding abort...");
        do {
            ret = curl_multi_wait(http->curlm, NULL, 0, 0, NULL);
            assert(ret == CURLM_OK);
            ret = curl_multi_perform(http->curlm, &count);
        } while (ret == CURLM_OK && count > 0);
        assert(count == 0);
        assert(ret == CURLM_OK);
    }

    http->curlbuf_off = 0;
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
    curl_easy_setopt(http->curl, CURLOPT_HEADERFUNCTION, sl_http_header_cb_);
    curl_easy_setopt(http->curl, CURLOPT_HEADERDATA, http);
#ifdef STREAMLIKE_DEBUG
    /* curl_easy_setopt(http->curl, CURLOPT_VERBOSE, 1); */
#endif
    http->http_off = 0;
    http->http_len = -1;
    http->http_status = 0;
    http->curlbuf_off = 0;
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
    SL_LOG("Attempting to read %zu bytes...", len);
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
                sl_http_set_state_(http, SL_HTTP_READY);
            }
            SL_LOG("Short read %zu.", http->outbuf_off);
            return http->outbuf_off;
        } else {
            if (http->state == SL_HTTP_READY) {
                sl_http_set_state_(http, SL_HTTP_WORKING);
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
    /* TODO: Implement seek_cur and seek_end. */
    /* TODO: Offset/whence sanity check. */
    /* TODO: Check if stream supports seeking. */
    char range_str[128];

    snprintf(range_str, sizeof(range_str), "%jd-", (intmax_t)offset);
    SL_LOG("Requesting range '%s'", range_str);

    sl_http_t *http = context;
    sl_cancel_transfer_(http);
    curl_easy_setopt(http->curl, CURLOPT_RANGE, range_str);

    http->http_off = offset;

    return 0;
}

off_t sl_http_tell_cb(void *context)
{
    sl_http_t *http = context;
    return http->http_off;
}

int sl_http_eof_cb(void *context)
{
    sl_http_t *http = context;
    return http->http_off >= http->http_len && http->http_len >= 0;
}

int sl_http_error_cb(void *context)
{
}

off_t sl_http_length_cb(void *context)
{
    sl_http_t *http = context;
    return http->http_len;
}

sl_seekable_t sl_http_seekable_cb(void *context)
{
}
