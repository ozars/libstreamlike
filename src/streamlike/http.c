#ifdef SL_DEBUG
#include "debug.h"
#endif

#ifndef SL_HTTP_ASSERT
# ifdef SL_ASSERT
#  define SL_HTTP_ASSERT(...) SL_ASSERT(__VA_ARGS__)
# else
#  define SL_HTTP_ASSERT(...) ((void)0)
# endif
#endif
#ifndef SL_HTTP_LOG
# ifdef SL_LOG
#  define SL_HTTP_LOG(...) SL_LOG(__VA_ARGS__)
# else
#  define SL_HTTP_LOG(...) ((void)0)
# endif
#endif
#include "http.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <curl/curl.h>

static pthread_mutex_t curl_global_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int curl_global_init_done = 0;

typedef struct sl_http_s
{
    off_t http_off;
    off_t http_len;
    int http_status;
    enum {
        SL_HTTP_RANGE_UNKNOWN,
        SL_HTTP_RANGE_YES,
        SL_HTTP_RANGE_NO
    } http_range_allowed;
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
            SL_HTTP_LOG("Setting state to READY.");
            break;
        case SL_HTTP_WORKING:
            SL_HTTP_LOG("Setting state to WORKING.");
            break;
        case SL_HTTP_PAUSED:
            SL_HTTP_LOG("Setting state to PAUSED.");
            break;
        case SL_HTTP_ABORT_REQUESTED:
            SL_HTTP_LOG("Setting state to ABORT_REQUESTED.");
            break;
        case SL_HTTP_ABORTED:
            SL_HTTP_LOG("Setting state to ABORTED.");
            break;
        default:
            SL_HTTP_LOG("Unexpected state: %d", state);
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
        SL_HTTP_LOG("Aborted.");
        return -1;
    }

    if (curlbuf_avail < outbuf_avail) {
        SL_HTTP_LOG("Partial read from curlbuf[%zu..%zu] to outbuf[%zu..%zu] "
                    "of length %zu.", http->curlbuf_off, curlbuf_size - 1,
                    http->outbuf_off, http->outbuf_size - 1, curlbuf_avail);
        memcpy(inp, outp, curlbuf_avail);
        http->outbuf_off += curlbuf_avail;
        http->curlbuf_off = 0;
        http->http_off += curlbuf_avail;
        return curlbuf_size;
    }
    SL_HTTP_LOG("Paused after read from curlbuf[%zu..%zu] to outbuf[%zu..%zu] "
                "of length %zu.", http->curlbuf_off, curlbuf_size - 1,
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
        SL_HTTP_LOG("Skipping all whitespaces header entry.");
        return curlbuf_size;
    }

    /* TODO: Is this scanf secure for a non-NULL terminated line? */
    if (sscanf(line, "HTTP/%*3s %d ", &http->http_status) == 1) {
        SL_HTTP_LOG("HTTP status read: %d", http->http_status);
        if (http->http_status == 416) {
            /* Range not satisfiable. */
            return 0;
        }
        return curlbuf_size;
    }

    /* If there is a colon... */
    const char *colon = memchr(line, ':', line_end - line);
    if (colon) {
        SL_HTTP_LOG("Parsing header line: '%.*s'", (int)(line_end - line),
                    line);

        /* Log and return if colon is the last character. */
        if (colon + 1 == line_end) {
            SL_HTTP_LOG("Skipping header entry with no value: '%.*s'",
                   (int)(line_end - line), line);
            return curlbuf_size;
        }

        /* Log and return if colon is the first character. */
        if (colon == line) {
            SL_HTTP_LOG("Skipping header entry with no key: '%.*s'",
                   (int)(line_end - line), line);
            return curlbuf_size;
        }

        /* Get the trimmed value and return if it's empty... */
        const char *value;
        if (memtrim(colon + 1, line_end - colon - 1, &value, NULL)) {
            SL_HTTP_LOG("Skipping header entry with empty value: '%.*s'",
                   (int)(line_end - line), line);
            return curlbuf_size;
        }

        /* Set length of the header key. */
        size_t key_len = colon - line;

        /* Headers of interests. */
        const char cnt_len[] = "Content-Length";
        const char cnt_rng[] = "Content-Range";
        const char act_rng[] = "Accept-Ranges";

        enum {
            HEADER_CONTENT_LENGTH,
            HEADER_CONTENT_RANGE,
            HEADER_ACCEPT_RANGES,
            HEADER_OTHER
        } header_type;

        if (memcasestartswith(line, key_len, cnt_rng, sizeof(cnt_rng) - 1)) {
            header_type = HEADER_CONTENT_RANGE;

        } else if(memcasestartswith(line, key_len, cnt_len,
                                    sizeof(cnt_len) - 1)) {
            header_type = HEADER_CONTENT_LENGTH;

        } else if(memcasestartswith(line, key_len, act_rng,
                                    sizeof(act_rng) - 1)) {
            header_type = HEADER_ACCEPT_RANGES;

        } else {
            header_type = HEADER_OTHER;

        }

        /* Set if range support isn't known. */
        if (http->http_range_allowed == SL_HTTP_RANGE_UNKNOWN) {
            if (header_type == HEADER_CONTENT_RANGE) {
                http->http_range_allowed = SL_HTTP_RANGE_YES;
            } else if (http->http_off != 0 && http->http_status == 200) {
                http->http_range_allowed = SL_HTTP_RANGE_NO;
            } else if (header_type == HEADER_ACCEPT_RANGES) {
                size_t value_len = line_end - value;
                if (memcasestartswith(value, value_len, "bytes", 5)) {
                    http->http_range_allowed = SL_HTTP_RANGE_YES;
                } else {
                    http->http_range_allowed = SL_HTTP_RANGE_NO;
                }
            }
        }

        /* If length isn't known... */
        if (http->http_len < 0) {

            /* If the content-range is found and status is 206 Partial Content,
             * read total length of stream. */
            if (http->http_status == 206
                    && header_type == HEADER_CONTENT_RANGE) {

                const char *length_part = memchr(value, '/', line_end - value);
                if (!length_part) {
                    SL_HTTP_LOG("Skipping header entry as there is no length "
                                "divider: '%.*s'", (int)(line_end - line),
                                line);
                    return curlbuf_size;
                }
                if (++length_part == line_end) {
                    SL_HTTP_LOG("Skipping header entry as length divider is at "
                                "the end: '%.*s'", (int)(line_end - line),
                                line);
                    return curlbuf_size;
                }
                http->http_len = strtoull(length_part, NULL, 10);
                SL_HTTP_LOG("Set http length to %jd from content range.",
                            (intmax_t)http->http_len);


            /* ...or if content-length is found and status is 200 OK, read total
             * length of the stream. */
            } else if (http->http_status == 200
                        && header_type == HEADER_CONTENT_LENGTH) {

                http->http_len = strtoull(value, NULL, 10);
                SL_HTTP_LOG("Set http length to %jd from content length.",
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
    SL_HTTP_LOG("Trying to cancel...");
    if (http->state == SL_HTTP_READY) {
        SL_HTTP_LOG("No need to cancel.");
        http->curlbuf_off = 0;
        return;
    }
    if (http->state == SL_HTTP_PAUSED) {
        SL_HTTP_LOG("Unpausing for cancellation...");
        sl_http_set_state_(http, SL_HTTP_ABORT_REQUESTED);
        ret = curl_easy_pause(http->curl, CURLPAUSE_CONT);
        SL_HTTP_ASSERT(ret == CURLE_WRITE_ERROR);
    } else {
        sl_http_set_state_(http, SL_HTTP_ABORT_REQUESTED);
    }

    if (http->state != SL_HTTP_ABORTED) {
        SL_HTTP_LOG("Concluding abort...");
        do {
            ret = curl_multi_wait(http->curlm, NULL, 0, 0, NULL);
            SL_HTTP_ASSERT(ret == CURLM_OK);
            ret = curl_multi_perform(http->curlm, &count);
        } while (ret == CURLM_OK && count > 0);
        SL_HTTP_ASSERT(count == 0);
        SL_HTTP_ASSERT(ret == CURLM_OK);
    }

    http->curlbuf_off = 0;
}

void sl_http_library_init()
{
    pthread_mutex_lock(&curl_global_init_mutex);
    if (!curl_global_init_done) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_global_init_done = 1;
        SL_HTTP_LOG("Initialized library.");
    }
    pthread_mutex_unlock(&curl_global_init_mutex);
}

void sl_http_library_cleanup()
{
    pthread_mutex_lock(&curl_global_init_mutex);
    if (curl_global_init_done) {
        curl_global_cleanup();
        curl_global_init_done = 0;
        SL_HTTP_LOG("Cleaned up library.");
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
    http->http_range_allowed = SL_HTTP_RANGE_UNKNOWN;
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
    CURLMcode curl_mret;
    http->outbuf = buffer;
    http->outbuf_off = 0;
    http->outbuf_size = len;
    int count = 1;
    SL_HTTP_LOG("Attempting to read %zu bytes...", len);
    if (http->state == SL_HTTP_PAUSED) {
        curl_easy_pause(http->curl, CURLPAUSE_CONT);
    }
    while (http->outbuf_off < http->outbuf_size) {
        CURLMcode result;
        if (curl_multi_wait(http->curlm, NULL, 0, 0, NULL) != CURLM_OK) {
            return http->outbuf_off;
        }
        curl_mret = curl_multi_perform(http->curlm, &count);
        if (curl_mret != CURLM_OK || !count) {
            #ifdef SL_DEBUG
            if (curl_mret != CURLM_OK) {
                SL_HTTP_LOG("cURL multi call returned error: %s (%d)",
                            curl_multi_strerror(curl_mret), curl_mret);
            }
            SL_HTTP_LOG("Checking handle result...");
            CURLMsg *result;
            int msgs;
            while (result = curl_multi_info_read(http->curlm, &msgs))
            {
                SL_LOG("MSGS: %d", msgs);
                if (result->msg == CURLMSG_DONE) {
                    SL_HTTP_LOG("cURL call returned: %s (%d)",
                                curl_easy_strerror(result->data.result),
                                result->data.result);
                } else {
                    SL_HTTP_LOG("Unknown message...");
                }
            }
            SL_LOG("MSGS: %d", msgs);
            #endif
            if (!count) {
                sl_http_set_state_(http, SL_HTTP_READY);
            }
            SL_HTTP_LOG("Short read %zu.", http->outbuf_off);
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

    if (offset < 0 && whence == SL_SEEK_SET) {
        return -1;
    }

    snprintf(range_str, sizeof(range_str), "%jd-", (intmax_t)offset);
    SL_HTTP_LOG("Requesting range '%s'", range_str);

    sl_http_t *http = context;
    sl_cancel_transfer_(http);
    curl_easy_setopt(http->curl, CURLOPT_RANGE, range_str);

    http->http_off = offset;
    http->curlbuf_off = 0;
    curl_multi_remove_handle(http->curlm, http->curl);
    curl_multi_add_handle(http->curlm, http->curl);

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
    /* TODO: Implement error handling. */
    return 0;
}

off_t sl_http_length_cb(void *context)
{
    sl_http_t *http = context;
    return http->http_len;
}

sl_seekable_t sl_http_seekable_cb(void *context)
{
    /* TODO: Send HEAD request if http_range_allowed is unknown. */
    sl_http_t *http = context;
    switch(http->http_range_allowed) {
        case SL_HTTP_RANGE_YES:
            return SL_SEEKING_SUPPORTED;
        default:
            return SL_SEEKING_NOT_SUPPORTED;
    }
}
