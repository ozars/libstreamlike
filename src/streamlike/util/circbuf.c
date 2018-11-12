#include "circbuf.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct circbuf_s
{
    void *data;
    size_t size;
    volatile size_t woff;
    volatile size_t roff;
    volatile int wdone;
    pthread_mutex_t wlock;
    pthread_cond_t  wcond;
    volatile int rdone;
    pthread_mutex_t rlock;
    pthread_cond_t  rcond;
};

circbuf_t* circbuf_init(size_t cbuf_size)
{
    circbuf_t* cbuf;

    if (cbuf_size == 0) {
        return NULL;
    }
    /* Increease buffer size by one, since woff can't be equal to roff when
     * buffer wraps around. (Otherwise it gets harder to track if buffer length
     * is equal to buffer size or zero.) */
    cbuf_size++;
    cbuf = malloc(sizeof(circbuf_t));
    if (!cbuf) {
        return NULL;
    }
    cbuf->data = malloc(cbuf_size);
    if (!cbuf->data) {
        free(cbuf);
        return NULL;
    }
    if (pthread_cond_init(&cbuf->rcond, NULL) != 0) {
        goto fail;
    }
    if (pthread_cond_init(&cbuf->wcond, NULL) != 0) {
        pthread_cond_destroy(&cbuf->rcond);
        goto fail;
    }
    if (pthread_mutex_init(&cbuf->rlock, NULL) != 0) {
        pthread_cond_destroy(&cbuf->rcond);
        pthread_cond_destroy(&cbuf->wcond);
        goto fail;
    }
    if (pthread_mutex_init(&cbuf->wlock, NULL) != 0) {
        pthread_cond_destroy(&cbuf->rcond);
        pthread_cond_destroy(&cbuf->wcond);
        pthread_mutex_destroy(&cbuf->rlock);
        goto fail;
    }
    cbuf->rdone = 0;
    cbuf->wdone = 0;

    /* Read offset and write offset. Don't confuse these with bob->ross. Common
     * mistake. */
    cbuf->woff = 0;
    cbuf->roff = 0;
    cbuf->size = cbuf_size;

    return (circbuf_t*)cbuf;

fail:
    free(cbuf->data);
    free(cbuf);
    return NULL;
}

void circbuf_destroy(circbuf_t* cbuf)
{
    pthread_mutex_destroy(&cbuf->rlock);
    pthread_mutex_destroy(&cbuf->wlock);
    pthread_cond_destroy(&cbuf->rcond);
    pthread_cond_destroy(&cbuf->wcond);
    free(cbuf->data);
    free(cbuf);
}

size_t circbuf_get_size(const circbuf_t* cbuf)
{
    return cbuf->size;
}

size_t circbuf_get_length(const circbuf_t* cbuf)
{
    size_t roff = cbuf->roff;
    size_t woff = cbuf->woff;
    if (woff >= roff) return woff - roff;
    return cbuf->size - roff + woff;
}

int circbuf_is_read_closed(const circbuf_t* cbuf)
{
    return cbuf->rdone;
}

int circbuf_is_write_closed(const circbuf_t* cbuf)
{
    return cbuf->wdone;
}

static
size_t read_some_(const void *cbuf_data, size_t cbuf_size, void *buf,
                  size_t buf_len, size_t *roffp, size_t woff)
{
    size_t avail;

    if (*roffp == woff) {
        return 0;
    }
    if (*roffp < woff) {
        avail = woff - *roffp;
        if (avail >= buf_len) {
            memcpy(buf, cbuf_data + *roffp, buf_len);
            *roffp += buf_len;
            return buf_len;
        }
        memcpy(buf, cbuf_data + *roffp, avail);
        *roffp += avail;
        return avail;
    }
    avail = cbuf_size - *roffp;
    if (avail >= buf_len) {
        memcpy(buf, cbuf_data + *roffp, buf_len);
        *roffp += buf_len;
        return buf_len;
    }
    memcpy(buf, cbuf_data + *roffp, avail);
    if (woff > buf_len - avail) {
        memcpy(buf + avail, cbuf_data, buf_len - avail);
        *roffp = buf_len - avail;
        return buf_len;
    }
    memcpy(buf + avail, cbuf_data, woff);
    *roffp = woff;
    return avail + woff;
}

size_t circbuf_read_some(circbuf_t *cbuf, void *buf, size_t buf_len)
{

    /* cbuf->roff isn't volatile from the viewpoint of consumer, since it's the
     * only consumer modifying it. So, work on non-volatile copy and update the
     * original later. */
    size_t roff = cbuf->roff;
    size_t read;

    /* Passing volatile cbuf->woff by value to freeze its value. */
    read = read_some_(cbuf->data, cbuf->size, buf, buf_len, &roff, cbuf->woff);

    /* Update cbuf->roff and signal producer. */
    pthread_mutex_lock(&cbuf->rlock);
    cbuf->roff = roff;
    pthread_cond_signal(&cbuf->rcond);
    pthread_mutex_unlock(&cbuf->rlock);
    return read;
}

size_t circbuf_read(circbuf_t *cbuf, void *buf, size_t buf_len)
{
    size_t read = 0;

    while (read < buf_len && (cbuf->roff != cbuf->woff || !cbuf->wdone)) {
        pthread_mutex_lock(&cbuf->wlock);
        while (cbuf->roff == cbuf->woff && !cbuf->wdone) {
            pthread_cond_wait(&cbuf->wcond, &cbuf->wlock);
        }
        pthread_mutex_unlock(&cbuf->wlock);
        read += circbuf_read_some(cbuf, buf + read, buf_len - read);
    }
    return read;
}

size_t circbuf_input_some(const circbuf_t *cbuf, const void **buf,
                          size_t buf_len)
{
    size_t roff = cbuf->roff;
    size_t woff = cbuf->woff;
    *buf = cbuf->data + roff;
    return (woff < roff ?
             (cbuf->size - roff > buf_len ? buf_len : cbuf->size - roff) :
             (woff - roff > buf_len ? buf_len : woff - roff));
}

size_t circbuf_dispose_some(circbuf_t *cbuf, size_t len)
{
    size_t roff;
    size_t cbuf_len = circbuf_get_length(cbuf);
    len = (cbuf_len < len ? cbuf_len : len);
    roff = cbuf->roff + len;
    if (roff >= cbuf->size) {
        roff -= cbuf->size;
    }
    /* Update cbuf->roff and signal producer. */
    pthread_mutex_lock(&cbuf->rlock);
    cbuf->roff = roff;
    pthread_cond_signal(&cbuf->rcond);
    pthread_mutex_unlock(&cbuf->rlock);
    return len;
}

static
size_t write_some_(void *cbuf_data, size_t cbuf_size, const void *buf,
                   size_t buf_len, size_t roff, size_t* woffp)
{
    size_t avail;

    /* If buffer is full. */
    if (*woffp + 1 == roff || (*woffp == cbuf_size - 1 && roff == 0)) {
        return 0;
    }
    /* If woff is before roff. */
    if (*woffp < roff) {
        avail =  roff - *woffp - 1;
        /* If there is enough data before roff. */
        if (avail >= buf_len) {
            memcpy(cbuf_data + *woffp, buf, buf_len);
            *woffp += buf_len;
            return buf_len;
        }
        /* Else, use whatever there is. */
        memcpy(cbuf_data + *woffp, buf, avail);
        *woffp += avail;
        return avail;
    }
    avail = cbuf_size - *woffp;
    /* Else if there is enough data until the end. */
    if (avail > buf_len) {
        memcpy(cbuf_data + *woffp, buf, buf_len);
        *woffp += buf_len;
        return buf_len;
    }
    /* If read offset is at the beginning, copy whatever there is until the end
     * except last byte. */
    if (roff == 0) {
        memcpy(cbuf_data + *woffp, buf, avail - 1);
        *woffp += avail - 1;
        return avail - 1;
    }
    /* Else copy whatever there is until the end. */
    memcpy(cbuf_data + *woffp, buf, avail);
    /* Check if there is enough data from beginning up to roff. */
    if (roff > buf_len - avail) {
        memcpy(cbuf_data, buf + avail, buf_len - avail);
        *woffp = buf_len - avail;
        return buf_len;
    }
    /* Else copy whatever there is up to roff. */
    memcpy(cbuf_data, buf + avail, roff - 1);
    *woffp = roff - 1;
    return avail + roff - 1;
}

size_t circbuf_write_some(circbuf_t *cbuf, const void *buf, size_t buf_len)
{

    /* cbuf->woff isn't volatile from the viewpoint of consumer, since it's the
     * only producer modifying it. So, work on non-volatile copy and update the
     * original later. */
    size_t woff = cbuf->woff;
    size_t written;

    /* Passing volatile cbuf->roff by value to freeze its value. */
    written = write_some_(cbuf->data, cbuf->size, buf, buf_len, cbuf->roff,
                          &woff);

    /* Update cbuf->woff and signal consumer. */
    pthread_mutex_lock(&cbuf->wlock);
    cbuf->woff = woff;
    pthread_cond_signal(&cbuf->wcond);
    pthread_mutex_unlock(&cbuf->wlock);
    return written;
}

size_t circbuf_write(circbuf_t *cbuf, const void *buf, size_t buf_len)
{
    size_t written = 0;

    if (cbuf->rdone) {
        return 0;
    }
    while (written < buf_len && !cbuf->rdone) {
        pthread_mutex_lock(&cbuf->rlock);
        while ((cbuf->woff + 1 == cbuf->roff
                    || (cbuf->woff + 1 == cbuf->size && cbuf->roff == 0))
               && !cbuf->rdone) {
            pthread_cond_wait(&cbuf->rcond, &cbuf->rlock);
        }
        pthread_mutex_unlock(&cbuf->rlock);
        if (!cbuf->rdone) {
            written += circbuf_write_some(cbuf, buf + written,
                                          buf_len - written);
        }
    }
    return written;
}

static
size_t write_some2_(void *cbuf_data, size_t cbuf_size,
                    circbuf_write_cb_t writer, void *context, size_t write_len,
                    size_t roff, size_t* woffp, char *eof_reached)
{
    /* Since this version of write will fetch from a user-provided feedback,
     * there is a chance that it will be fed less number bytes than it needs,
     * meaning that either eof is reached or some error happened in user
     * provided input. Therefore, it should set the status in eof_reached
     * argument before returning, to indicate shortage is due to user-provided
     * input, not due to buffer being full at the moment. */
    #define WRITE_AND_RETURN_IF_EOF_(there, length) \
        written = writer(context, there, length); \
        *woffp += written; \
        *woffp %= cbuf_size; \
        total_written += written; \
        if (written < length) { \
            if (eof_reached) { \
                *eof_reached = 1; \
            } \
            return total_written; \
        }
    #define WRITE_AND_RETURN_(there, length) \
        WRITE_AND_RETURN_IF_EOF_(there, length); \
        return total_written;

    size_t avail;
    size_t written;
    size_t total_written = 0;

    /* If buffer is full. */
    if (*woffp + 1 == roff || (*woffp == cbuf_size - 1 && roff == 0)) {
        return 0;
    }
    /* If woff is before roff. */
    if (*woffp < roff) {
        avail =  roff - *woffp - 1;
        /* If there is enough data before roff. */
        if (avail >= write_len) {
            WRITE_AND_RETURN_(cbuf_data + *woffp, write_len);
        }
        /* Else, use whatever there is. */
        WRITE_AND_RETURN_(cbuf_data + *woffp, avail);
    }
    avail = cbuf_size - *woffp;
    /* Else if there is enough data until the end. */
    if (avail > write_len) {
        WRITE_AND_RETURN_(cbuf_data + *woffp, write_len);
    }
    /* If read offset is at the beginning, copy whatever there is until the end
     * except last byte. */
    if (roff == 0) {
        WRITE_AND_RETURN_(cbuf_data + *woffp, avail - 1);
    }
    /* Else copy whatever there is until the end. */
    WRITE_AND_RETURN_IF_EOF_(cbuf_data + *woffp, avail);
    /* Check if there is enough data from beginning up to roff. */
    if (roff > write_len - avail) {
        WRITE_AND_RETURN_(cbuf_data, write_len - avail);
    }
    /* Else copy whatever there is up to roff. */
    WRITE_AND_RETURN_(cbuf_data, roff - 1);

    #undef WRITE_AND_RETURN_IF_EOF_
    #undef WRITE_AND_RETURN_
}

size_t circbuf_write_some2(circbuf_t *cbuf, circbuf_write_cb_t writer,
                           void *context, size_t write_len, char *eof_reached)
{
    /* cbuf->woff isn't volatile from the viewpoint of consumer, since it's the
     * only producer modifying it. So, work on non-volatile copy and update the
     * original later. */
    size_t woff = cbuf->woff;
    size_t written;

    /* Passing volatile cbuf->roff by value to freeze its value. */
    written = write_some2_(cbuf->data, cbuf->size, writer, context, write_len,
                           cbuf->roff, &woff, eof_reached);

    /* Update cbuf->woff and signal consumer. */
    pthread_mutex_lock(&cbuf->wlock);
    cbuf->woff = woff;
    pthread_cond_signal(&cbuf->wcond);
    pthread_mutex_unlock(&cbuf->wlock);
    return written;

}

size_t circbuf_write2(circbuf_t *cbuf, circbuf_write_cb_t writer, void *context,
                      size_t write_len)
{
    char eof_reached = 0;
    size_t written = 0;

    if (cbuf->rdone) {
        return 0;
    }
    while (written < write_len && !cbuf->rdone && !eof_reached) {
        pthread_mutex_lock(&cbuf->rlock);
        while ((cbuf->woff + 1 == cbuf->roff
                    || (cbuf->woff + 1 == cbuf->size && cbuf->roff == 0))
               && !cbuf->rdone) {
            pthread_cond_wait(&cbuf->rcond, &cbuf->rlock);
        }
        pthread_mutex_unlock(&cbuf->rlock);
        if (!cbuf->rdone) {
            written += circbuf_write_some2(cbuf, writer, context,
                                           write_len - written, &eof_reached);
        }
    }
    return written;
}

int circbuf_close_read(circbuf_t *cbuf)
{

    if (cbuf->rdone) {
        return -1;
    }

    /* Update cbuf->rdone and signal producer. */
    pthread_mutex_lock(&cbuf->rlock);
    cbuf->rdone = 1;
    pthread_cond_signal(&cbuf->rcond);
    pthread_mutex_unlock(&cbuf->rlock);
    return 0;
}

int circbuf_close_write(circbuf_t *cbuf)
{

    if (cbuf->wdone) {
        return -1;
    }

    /* Update cbuf->wdone and signal consumer. */
    pthread_mutex_lock(&cbuf->wlock);
    cbuf->wdone = 1;
    pthread_cond_signal(&cbuf->wcond);
    pthread_mutex_unlock(&cbuf->wlock);
    return 0;
}
