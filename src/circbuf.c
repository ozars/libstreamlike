#include "circbuf.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct circbuf_s_
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
} circbuf_t_;

circbuf_t* circbuf_init(size_t cbuf_size)
{
    circbuf_t_* cbuf;

    if (cbuf_size == 0) {
        return NULL;
    }
    cbuf = malloc(sizeof(circbuf_t_));
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

void circbuf_destroy(circbuf_t* cbuf_opq)
{
    circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;
    pthread_mutex_destroy(&cbuf->rlock);
    pthread_mutex_destroy(&cbuf->wlock);
    pthread_cond_destroy(&cbuf->rcond);
    pthread_cond_destroy(&cbuf->wcond);
    free(cbuf->data);
    free(cbuf);
}

size_t circbuf_get_size(const circbuf_t* cbuf_opq)
{
    circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;
    return cbuf->size;
}

size_t circbuf_get_length(const circbuf_t* cbuf_opq)
{
    circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;
    size_t roff = cbuf->roff;
    size_t woff = cbuf->woff;
    if (woff >= roff) return woff - roff;
    return cbuf->size - roff + woff;
}

int circbuf_is_read_closed(const circbuf_t* cbuf_opq)
{
    circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;
    return cbuf->rdone;
}

int circbuf_is_write_closed(const circbuf_t* cbuf_opq)
{
    circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;
    return cbuf->wdone;
}

static
size_t circbuf_read_some_(const void *cbuf_data, size_t cbuf_size,
                          void *buf, size_t buf_len,
                          size_t *roffp, size_t woff)
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
        memcpy(buf, cbuf_data, buf_len - avail);
        *roffp = buf_len - avail;
        return buf_len;
    }
    memcpy(buf, cbuf_data, woff);
    *roffp = woff;
    return avail + woff;
}

size_t circbuf_read_some(circbuf_t *cbuf_opq, void *buf, size_t buf_len)
{
    circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;

    /* cbuf->roff isn't volatile from the viewpoint of consumer, since it's the
     * only consumer. So, work on non-volatile local copy and update later. */
    size_t roff = cbuf->roff;
    size_t read;

    /* Passing volatile cbuf->woff by value to freeze its value. */
    read = circbuf_read_some_(cbuf->data, cbuf->size, buf, buf_len, &roff,
                              cbuf->woff);

    /* Update cbuf->roff and signal producer. */
    pthread_mutex_lock(&cbuf->rlock);
    cbuf->roff = roff;
    pthread_cond_signal(&cbuf->rcond);
    pthread_mutex_unlock(&cbuf->rlock);
    return read;
}

size_t circbuf_read(circbuf_t *cbuf_opq, void *buf, size_t buf_len)
{
    circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;
    size_t read = 0;

    if (cbuf->wdone) {
        return 0;
    }
    while (read < buf_len && !cbuf->wdone) {
        pthread_mutex_lock(&cbuf->wlock);
        while (cbuf->roff == cbuf->woff && !cbuf->wdone) {
            pthread_cond_wait(&cbuf->wcond, &cbuf->wlock);
        }
        pthread_mutex_unlock(&cbuf->wlock);
        if (!cbuf->wdone) {
            read += circbuf_read_some(cbuf_opq, buf + read, buf_len - read);
        }
    }
    return read;
}

size_t circbuf_input_some(const circbuf_t *cbuf_opq, const void **buf,
                          size_t buf_len)
{
    const circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;
    size_t roff = cbuf->roff;
    *buf = cbuf->data + roff;
    return (cbuf->size - roff > buf_len ? buf_len : cbuf->size - roff);
}

size_t circbuf_dispose_some(circbuf_t *cbuf_opq, size_t len)
{
    circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;
    size_t roff;
    size_t cbuf_len = circbuf_get_length(cbuf_opq);
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
size_t circbuf_write_some_(void *cbuf_data, size_t cbuf_size,
                           const void *buf, size_t buf_len,
                           size_t roff, size_t* woffp)
{
    size_t avail;

    if (*woffp + 1 == roff || (*woffp == cbuf_size - 1 && roff == 0)) {
        return 0;
    }
    if (*woffp < roff) {
        avail =  roff - *woffp;
        if (avail >= buf_len) {
            memcpy(cbuf_data + *woffp, buf, buf_len);
            *woffp += buf_len;
            return buf_len;
        }
        memcpy(cbuf_data + *woffp, buf, avail);
        *woffp += avail;
        return avail;
    }
    avail = cbuf_size - *woffp;
    if (avail >= buf_len) {
        memcpy(cbuf_data + *woffp, buf, buf_len);
        *woffp += buf_len;
        return buf_len;
    }
    memcpy(cbuf_data + *woffp, buf, avail);
    if (roff > buf_len - avail + 1) {
        memcpy(cbuf_data, buf, buf_len - avail);
        *woffp = buf_len - avail;
        return buf_len;
    }
    memcpy(cbuf_data, buf, roff - 1);
    *woffp = roff - 1;
    return avail + roff - 1;
}

size_t circbuf_write_some(circbuf_t *cbuf_opq, const void *buf, size_t buf_len)
{
    circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;

    /* cbuf->roff isn't volatile from the viewpoint of consumer, since it's the
     * only consumer. So, work on non-volatile local copy and update later. */
    size_t woff = cbuf->woff;
    size_t written;

    /* Passing volatile cbuf->roff by value to freeze its value. */
    written = circbuf_write_some_(cbuf->data, cbuf->size, buf, buf_len,
                                  cbuf->roff, &woff);

    /* Update cbuf->woff and signal consumer. */
    pthread_mutex_lock(&cbuf->wlock);
    cbuf->woff = woff;
    pthread_cond_signal(&cbuf->wcond);
    pthread_mutex_unlock(&cbuf->wlock);
    return written;
}

size_t circbuf_write(circbuf_t *cbuf_opq, const void *buf, size_t buf_len)
{
    circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;
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
            written += circbuf_write_some(cbuf_opq, buf + written,
                                          buf_len - written);
        }
    }
    return written;
}

int circbuf_conc_close_read(circbuf_t *cbuf_opq)
{
    circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;

    if (cbuf->rdone) {
        return -1;
    }

    /* Update cbuf->wdone and signal producer. */
    pthread_mutex_lock(&cbuf->rlock);
    cbuf->rdone = 1;
    pthread_cond_signal(&cbuf->rcond);
    pthread_mutex_unlock(&cbuf->rlock);
    return 0;
}

int circbuf_conc_close_write(circbuf_t *cbuf_opq)
{
    circbuf_t_ *cbuf = (circbuf_t_*)cbuf_opq;

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