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
#include "buffer.h"

#include <stdlib.h>
#include <pthread.h>

#include "util/circbuf.h"

typedef struct sl_buffer_s
{
    streamlike_t* inner_stream;
    circbuf_t* cbuf;
    pthread_t* filler;
    off_t pos;
    size_t step_size;
    /* TODO: Seperate eof from failure. */
    int eof;
    pthread_mutex_t* seek_lock;
    pthread_cond_t* seek_cond;
    int seek_requested;
    off_t seek_off;
    int seek_whence;
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

    SL_BUFFER_LOG("Started.");

    /* Loop until read is closed and there is no outstanding seek request. */
    while (!circbuf_is_read_closed(context->cbuf)
            || context->seek_requested) {

        /* Handle seek if requested. */
        if (context->seek_requested) {
            SL_BUFFER_LOG("Received seek request.");
            /* Seek and store the result. */
            context->seek_result = sl_seek(context->inner_stream,
                                           context->seek_off,
                                           context->seek_whence);

            /* LOCK SEEK OPERATONS */
            pthread_mutex_lock(context->seek_lock);

            /* Reset circbuf if seek is successful. */
            if (context->seek_result == 0) {
                /* TODO: Could be handled more efficiently without requiring
                 * rebuffering some data after seek. Let's keep it simple for
                 * now. */
                circbuf_reset(context->cbuf);
            }

            /* Clear the seek request flag. */
            context->seek_requested = 0;

            /* Signal the consumer who requested seek. */
            SL_BUFFER_LOG("Signaling consumer...");
            pthread_cond_signal(context->seek_cond);

            /* UNLOCK SEEK OPERATONS */
            pthread_mutex_unlock(context->seek_lock);

            SL_BUFFER_LOG("Served seek request.");
        }

        SL_BUFFER_LOG("Writing to circbuf.");
        /* Write to buffer from stream. */
        written = circbuf_write2(context->cbuf, filler_cb,
                                 context->inner_stream,
                                 context->step_size);
        SL_BUFFER_LOG("Wrote %zd bytes to circbuf.", written);

        /* If there is an error or eof is reached... */
        if (written < context->step_size) {

            /* LOCK SEEK OPERATONS */
            pthread_mutex_lock(context->seek_lock);

            /* Signal consumer that writing is closed so that reading does not
             * block at the end of file.. */
            circbuf_close_write(context->cbuf);
            SL_BUFFER_LOG("Closed writing.");

            /* If buffer not closed, wait until it is closed by either
             * sl_buffer_destroy() or sl_buffer_seek_cb(). */
            if (!circbuf_is_read_closed(context->cbuf)) {
                SL_BUFFER_LOG("Waiting on condition variable.");
                pthread_cond_wait(context->seek_cond, context->seek_lock);
                SL_BUFFER_LOG("Waited on condition variable.");
            }

            /* UNLOCK SEEK OPERATONS */
            pthread_mutex_unlock(context->seek_lock);
        }
    }
    SL_BUFFER_LOG("Exiting...");
    return NULL;
}

streamlike_t* sl_buffer_create(streamlike_t* inner_stream)
{
    return sl_buffer_create2(inner_stream, SL_BUFFER_DEFAULT_BUFFER_SIZE,
                             SL_BUFFER_DEFAULT_STEP_SIZE);
}

streamlike_t* sl_buffer_create2(streamlike_t* inner_stream, size_t buffer_size,
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
    context->pos          = 0;
    context->step_size    = step_size;

    context->eof = 0;

    context->seek_lock = seek_lock;
    context->seek_cond = seek_cond;

    context->seek_requested = 0;
    context->seek_off       = 0;
    context->seek_whence    = 0;
    context->seek_result    = 0;

    stream->context = context;

    stream->read   = sl_buffer_read_cb;
    stream->input  = sl_buffer_input_cb;
    stream->seek   = sl_buffer_seek_cb;
    stream->tell   = sl_buffer_tell_cb;
    stream->eof    = sl_buffer_eof_cb;
    stream->error  = sl_buffer_error_cb;
    stream->length = sl_buffer_length_cb;

    stream->seekable     = sl_buffer_seekable_cb;
    stream->ckp_count    = sl_buffer_ckp_count_cb;
    stream->ckp          = sl_buffer_ckp_cb;
    stream->ckp_offset   = sl_buffer_ckp_offset_cb;
    stream->ckp_metadata = sl_buffer_ckp_metadata_cb;

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
        if (context->filler == NULL) {
            SL_BUFFER_LOG("Skipping deallocating filling thread since it's "
                          "NULL.\n");
        } else {
            /* Signal filer thread to close. */
            pthread_mutex_lock(context->seek_lock);

            /* Close reading on circular buffer. */
            circbuf_close_read(context->cbuf);

            /* Note: There shouldn't be any outstanding seek request since
             * there is only one consumer, the only possible caller of both
             * this function and sl_seek. */
            SL_BUFFER_ASSERT(context->seek_requested == 0);

            /* Signal producer if it has reached EOF. */
            pthread_cond_signal(context->seek_cond);
            pthread_mutex_unlock(context->seek_lock);

            /* Join the thread. */
            ret = pthread_join(*context->filler, NULL);
            if (ret != 0) {
                SL_BUFFER_LOG("ERROR: Couldn't join filler thread (%d).\n",
                              ret);
                return -1;
            }
            free(context->filler);
            context->filler = NULL;
        }

        /* Destroy circular buffer. */
        if (context->cbuf == NULL) {
            SL_BUFFER_LOG("Skipping deallocating circular buffer since it's "
                          "NULL.\n");
        } else {
            circbuf_destroy(context->cbuf);
            context->cbuf = NULL;
        }

        /* Destroy eof/seek locks and condition variables. */
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
    /* TODO: I don't remember why I defined this function. :) */
    return 0;
}

size_t sl_buffer_read_cb(void *context, void *buffer, size_t len)
{
    SL_BUFFER_ASSERT(context);
    sl_buffer_t *stream = context;

    size_t read = circbuf_read(stream->cbuf, buffer, len);
    if (read < len) {
        stream->eof = 1;
    }
    stream->pos += read;
    return read;
}

size_t sl_buffer_input_cb(void *context, const void **buffer, size_t size)
{
    /* TODO */
    SL_BUFFER_ASSERT(context);
    sl_buffer_t *stream = context;
    return 0;
}

int sl_buffer_seek_cb(void *context, off_t offset, int whence)
{
    SL_BUFFER_ASSERT(context);
    SL_BUFFER_ASSERT(whence == SL_SEEK_SET); /* TODO: Other whences are to be
                                                implemened. */
    sl_buffer_t *stream = context;

    /* Set seek parameters. */
    stream->seek_off = offset;
    stream->seek_whence = whence;

    pthread_mutex_lock(stream->seek_lock);

    /* Set seek request flag. */
    stream->seek_requested = 1;

    /* Signal that reading is closed, so that writing to circular buffer ceases
     * blocking if it is doing so. Circbuf will be already reset after seeking
     * is done in fill_buffer(). */
    circbuf_close_read(stream->cbuf);

    /* Signal producer if it's waiting on EOF. */
    SL_BUFFER_LOG("Signaling producer...");
    pthread_cond_signal(stream->seek_cond);

    /* Wait for seeking to be completed. */
    SL_BUFFER_LOG("Waiting for seeking...");
    pthread_cond_wait(stream->seek_cond, stream->seek_lock);
    SL_BUFFER_LOG("Done waiting for seeking...");
    pthread_mutex_unlock(stream->seek_lock);

    /* If successful, update offset, clear eof and return success. */
    if (stream->seek_result == 0) {
        stream->pos = offset;
        stream->eof = 0;
        return 0;
    }

    SL_ASSERT(stream->seek_result == 0);

    /* Else return error code. */
    return stream->seek_result;
}

off_t sl_buffer_tell_cb(void *context)
{
    SL_BUFFER_ASSERT(context);
    sl_buffer_t *stream = context;
    return stream->pos;
}

int sl_buffer_eof_cb(void *context)
{
    SL_BUFFER_ASSERT(context);
    sl_buffer_t *stream = context;
    return stream->eof;
}
int sl_buffer_error_cb(void *context)
{
    return 0;
}

off_t sl_buffer_length_cb(void *context)
{
    return 0;
}


sl_seekable_t sl_buffer_seekable_cb(void *context)
{
    return 0;
}

int sl_buffer_ckp_count_cb(void *context)
{
    return 0;
}

const sl_ckp_t* sl_buffer_ckp_cb(void *context, int idx)
{
    return NULL;
}

off_t sl_buffer_ckp_offset_cb(void *context, const sl_ckp_t *ckp)
{
    return 0;
}

size_t sl_buffer_ckp_metadata_cb(void *context, const sl_ckp_t *ckp,
                                 const void **result)
{
    return 0;
}
