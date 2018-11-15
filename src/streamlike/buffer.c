#ifdef SL_DEBUG
# include "debug.h"
#endif

#ifndef SL_BUFFER_ASSERT
# ifdef SL_ASSERT
#  define SL_BUFFER_ASSERT(...) SL_ASSERT(__VA_ARGS__)
# else
#  define SL_BUFFER_ASSERT(...) ((void)0)
# endif
#endif
#ifndef SL_BUFFER_LOG
# ifdef SL_LOG
#  define SL_BUFFER_LOG(...) SL_LOG(__VA_ARGS__)
# else
#  define SL_BUFFER_LOG(...) ((void)0)
# endif
#endif

#include "../streamlike.h"
#include "buffer.h"

#include <stdlib.h>
#include <pthread.h>
#include "util/circbuf.h"

typedef struct sl_buffer_s
{
    streamlike_t* inner_stream;
    circbuf_t* cbuf;
    pthread_t* filler;
    size_t step_size;
    pthread_mutex_t* eof_lock;
    pthread_cond_t* eof_cond;
    int eof_reached;
    pthread_mutex_t* seek_lock;
    pthread_cond_t* seek_cond;
    int seek_requested;
    off_t seek_off;
    int seek_pos;
    int seek_result;
} sl_buffer_t;

static
size_t filler_cb(void *context, void *buf, size_t len)
{
    return sl_read(context, buf, len);
}

static
void* fill_buffer(void *arg)
{
    sl_buffer_t *context = (sl_buffer_t*) arg;
    size_t written;
    while (!circbuf_is_read_closed(context->cbuf)) {

        /* Handle seek if requested. */
        if (context->seek_requested) {
            /* Seek and store the result. */
            context->seek_result = sl_seek(context->inner_stream,
                                           context->seek_off,
                                           context->seek_pos);

            /* Reset circbuf if seek is successful. */
            if (context->seek_result == 0) {
                /* TODO: Could be handled more efficiently without requiring
                 * rebuffering some data after seek. Let's keep it simple for
                 * now. */
                circbuf_reset(context->cbuf);
            }

            /* Signal the consumer who requested seek. */
            pthread_mutex_lock(context->seek_lock);
            context->seek_requested = 0;
            pthread_cond_signal(context->seek_cond);
            pthread_mutex_unlock(context->seek_lock);
        }
        /* Write to buffer from stream. */
        written = circbuf_write2(context->cbuf, filler_cb,
                                 context->inner_stream,
                                 context->step_size);

        /* If there is an error or eof is reached... */
        if (written < context->step_size) {
            /* Wait until buffer is closed or a seek is requested. */
            pthread_mutex_lock(context->eof_lock);
            context->eof_reached = 1;
            if (!circbuf_is_read_closed(context->cbuf)) {
                pthread_cond_wait(context->eof_cond, context->eof_lock);
            }
            pthread_mutex_unlock(context->eof_lock);
        }
    }
    return NULL;
}

streamlike_t* sl_buffer_create(streamlike_t* inner_stream, size_t buffer_size,
                               size_t step_size)
{
    streamlike_t* stream = NULL;
    sl_buffer_t* context = NULL;
    circbuf_t *cbuf      = NULL;
    pthread_mutex_t* eof_lock  = NULL;
    pthread_cond_t* eof_cond   = NULL;
    pthread_mutex_t* seek_lock = NULL;
    pthread_cond_t* seek_cond  = NULL;

    if (inner_stream == NULL) {
        SL_BUFFER_LOG("ERROR: Inner stream can't be NULL.");
        return NULL;
    }

    if (buffer_size == 0) {
        SL_BUFFER_LOG("ERROR: Buffer size should be greater than zero.");
        return NULL;
    }

    if (step_size == 0) {
        /* TODO: Zero step size may default to some value in future. */
        SL_BUFFER_LOG("ERROR: Buffer size should be greater than zero.");
        return NULL;
    }

    context = malloc(sizeof(sl_buffer_t));
    if (context == NULL) {
        SL_BUFFER_LOG("ERROR: Couldn't allocate memory for streamlike buffer "
                      "context.\n");
        goto fail;
    }

    cbuf = circbuf_init(buffer_size);
    if (cbuf == NULL) {
        SL_BUFFER_LOG("ERROR: Couldn't initialize circular buffer of size %zu."
                      "\n", buffer_size);
        goto fail;
    }

    stream = malloc(sizeof(streamlike_t));
    if (stream == NULL) {
        SL_BUFFER_LOG("ERROR: Couldn't allocate memory for streamlike buffer."
                      "\n");
        goto fail;
    }

    eof_lock = malloc(sizeof(pthread_mutex_t));
    if (eof_lock == NULL || pthread_mutex_init(eof_lock, NULL) != 0) {
        SL_BUFFER_LOG("ERROR: Couldn't initialize EOF mutex.\n");
        goto fail;
    }

    eof_cond = malloc(sizeof(pthread_cond_t));
    if (eof_cond == NULL || pthread_cond_init(eof_cond, NULL) != 0) {
        SL_BUFFER_LOG("ERROR: Couldn't initialize EOF condition variable.\n");
        goto fail;
    }

    seek_lock = malloc(sizeof(pthread_mutex_t));
    if (seek_lock == NULL || pthread_mutex_init(seek_lock, NULL) != 0) {
        SL_BUFFER_LOG("ERROR: Couldn't initialize seek mutex.\n");
        goto fail;
    }

    seek_cond = malloc(sizeof(pthread_cond_t));
    if (seek_cond == NULL || pthread_cond_init(seek_cond, NULL) != 0) {
        SL_BUFFER_LOG("ERROR: Couldn't initialize seek condition variable.\n");
        goto fail;
    }

    context->inner_stream = inner_stream;
    context->cbuf         = cbuf;
    context->filler       = NULL;
    context->step_size    = step_size;

    context->eof_lock  = eof_lock;
    context->eof_cond  = eof_cond;
    context->seek_lock = seek_lock;
    context->seek_cond = seek_cond;

    context->eof_reached    = 0;
    context->seek_requested = 0;
    context->seek_off       = 0;
    context->seek_pos       = 0;
    context->seek_result    = 0;

    stream->context = context;

#define SET_PASSTHROUGH_IF_NOT_NULL_(op) \
    if (inner_stream->op == NULL) { \
        stream->op = NULL; \
    } else { \
        stream->op = sl_buffer_ ## op ## _cb; \
    }

    SET_PASSTHROUGH_IF_NOT_NULL_(read);
    SET_PASSTHROUGH_IF_NOT_NULL_(input);
    SET_PASSTHROUGH_IF_NOT_NULL_(write);
    SET_PASSTHROUGH_IF_NOT_NULL_(flush);
    SET_PASSTHROUGH_IF_NOT_NULL_(seek);
    SET_PASSTHROUGH_IF_NOT_NULL_(tell);
    SET_PASSTHROUGH_IF_NOT_NULL_(eof);
    SET_PASSTHROUGH_IF_NOT_NULL_(error);
    SET_PASSTHROUGH_IF_NOT_NULL_(length);

    SET_PASSTHROUGH_IF_NOT_NULL_(seekable);
    SET_PASSTHROUGH_IF_NOT_NULL_(ckp_count);
    SET_PASSTHROUGH_IF_NOT_NULL_(ckp);
    SET_PASSTHROUGH_IF_NOT_NULL_(ckp_offset);
    SET_PASSTHROUGH_IF_NOT_NULL_(ckp_metadata);

#undef SET_PASSTHROUGH_IF_NOT_NULL_

    return stream;

fail:
    free(stream);
    free(context);
    free(cbuf);
    if (eof_lock) {
        pthread_mutex_destroy(eof_lock);
        free(eof_lock);
    }
    if (eof_cond) {
        pthread_cond_destroy(eof_cond);
        free(eof_cond);
    }
    if (seek_lock) {
        pthread_mutex_destroy(seek_lock);
        free(seek_lock);
    }
    if (seek_cond) {
        pthread_cond_destroy(seek_cond);
        free(seek_cond);
    }
    return NULL;
}

int sl_buffer_destroy(streamlike_t *buffer_stream)
{
    sl_buffer_t* context;
    int ret;

    if (buffer_stream == NULL) {
        SL_BUFFER_LOG("Buffer stream is NULL. Skipping destroy.\n");
        return 0;
    }

    context = (sl_buffer_t*)buffer_stream->context;

    if (context == NULL) {
        SL_BUFFER_LOG("Buffer stream context is NULL. Skipping destroy.\n");
    } else {
        if (context->cbuf == NULL) {
            SL_BUFFER_LOG("Skipping deallocating circular buffer since it's "
                          "NULL.\n");
        } else {
            circbuf_destroy(context->cbuf);
            context->cbuf = NULL;
        }

        if (context->filler == NULL) {
            SL_BUFFER_LOG("Skipping deallocating filling thread since it's "
                          "NULL.\n");
        } else {
            ret = pthread_join(*context->filler, NULL);
            if (ret != 0) {
                SL_BUFFER_LOG("ERROR: Couldn't join filler thread (%d).\n",
                              ret);
                return -1;
            }
            free(context->filler);
            context->filler = NULL;
        }

        if (context->eof_lock == NULL) {
            SL_BUFFER_LOG("Skipping deallocating eof mutex since it's NULL.\n");
        } else {
            ret = pthread_mutex_destroy(context->eof_lock);
            if (ret != 0) {
                SL_BUFFER_LOG("ERROR: Couldn't destroy eof mutex (%d).\n", ret);
                return -1;
            }
            free(context->eof_lock);
            context->eof_lock = NULL;
        }

        if (context->eof_cond == NULL) {
            SL_BUFFER_LOG("Skipping deallocating eof condition variable since "
                          "it's NULL.\n");
        } else {
            ret = pthread_cond_destroy(context->eof_cond);
            if (ret != 0) {
                SL_BUFFER_LOG("ERROR: Couldn't destroy eof condition variable "
                              "(%d).\n", ret);
                return -1;
            }
            free(context->eof_cond);
            context->eof_cond = NULL;
        }

        if (context->seek_lock == NULL) {
            SL_BUFFER_LOG("Skipping deallocating seek mutex since it's NULL."
                          "\n");
        } else {
            ret = pthread_mutex_destroy(context->seek_lock);
            if (ret != 0) {
                SL_BUFFER_LOG("ERROR: Couldn't destroy seek mutex (%d)."
                              "\n", ret);
                return -1;
            }
            free(context->seek_lock);
            context->seek_lock = NULL;
        }

        if (context->seek_cond == NULL) {
            SL_BUFFER_LOG("Skipping deallocating eof condition variable since "
                          "it's NULL.\n");
        } else {
            ret = pthread_cond_destroy(context->seek_cond);
            if (ret != 0) {
                SL_BUFFER_LOG("ERROR: Couldn't destroy eof condition variable "
                              "(%d).\n", ret);
                return -1;
            }
            free(context->seek_cond);
            context->seek_cond = NULL;
        }
        free(context);
    }
    free(buffer_stream);
    return 0;
}

int sl_buffer_threaded_fill_buffer(streamlike_t *buffer_stream)
{
    sl_buffer_t* context;
    int ret;

    if (buffer_stream == NULL) {
        SL_BUFFER_LOG("ERROR: Buffer stream is NULL.");
        return -1;
    }

    context = (sl_buffer_t*)buffer_stream->context;

    if (context == NULL) {
        SL_BUFFER_LOG("ERROR: Context is NULL.");
        return -2;
    }

    if (context->cbuf == NULL) {
        SL_BUFFER_LOG("ERROR: Circular buffer is NULL.");
        return -3;
    }

    if (context->filler != NULL) {
        SL_BUFFER_LOG("ERROR: There is already a filler thread running.");
        return -4;
    }
    context->filler = malloc(sizeof(pthread_t));

    ret = pthread_create(context->filler, NULL, fill_buffer,
                         buffer_stream->context);
    if (ret != 0) {
        SL_BUFFER_LOG("ERROR: Couldn't create filler thread (%d).", ret);
        free(context->filler);
        context->filler = NULL;
        return -5;
    }
    return 0;
}

int sl_buffer_blocking_fill_buffer(streamlike_t *buffer_stream)
{
    sl_buffer_t* context;

    if (buffer_stream == NULL) {
        SL_BUFFER_LOG("ERROR: Buffer stream is NULL.");
        return -1;
    }

    context = (sl_buffer_t*)buffer_stream->context;

    if (context == NULL) {
        SL_BUFFER_LOG("ERROR: Context is NULL.");
        return -2;
    }

    (void)fill_buffer(context);

    return 0;
}

int sl_buffer_close_buffer(streamlike_t *buffer_stream)
{
    return 0;
}

size_t sl_buffer_read_cb(void *context, void *buffer, size_t len);
size_t sl_buffer_input_cb(void *context, const void **buffer, size_t size);
size_t sl_buffer_write_cb(void *context, const void *buffer, size_t size);
int sl_buffer_flush_cb(void *context);
int sl_buffer_seek_cb(void *context, off_t offset, int whence);
off_t sl_buffer_tell_cb(void *context);
int sl_buffer_eof_cb(void *context);
int sl_buffer_error_cb(void *context);
off_t sl_buffer_length_cb(void *context);

sl_seekable_t sl_buffer_seekable_cb(void *context);
int sl_buffer_ckp_count_cb(void *context);
const sl_ckp_t* sl_buffer_ckp_cb(void *context, int idx);
off_t sl_buffer_ckp_offset_cb(void *context, const sl_ckp_t *ckp);
size_t sl_buffer_ckp_metadata_cb(void *context, const sl_ckp_t *ckp,
                                 const void **result);

