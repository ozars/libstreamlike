#ifndef CIRCBUF_H
#define CIRCBUF_H
#include <pthread.h>

typedef
size_t (*circbuf_write_cb_t)(void *context, void *buf, size_t buf_len);

typedef struct circbuf_s circbuf_t;

circbuf_t* circbuf_init(size_t buf_len);
void circbuf_destroy(circbuf_t* cbuf);

size_t circbuf_get_size(const circbuf_t* cbuf);
size_t circbuf_get_length(const circbuf_t* cbuf);
int circbuf_is_read_closed(const circbuf_t* cbuf);
int circbuf_is_write_closed(const circbuf_t* cbuf);

size_t circbuf_read_some(circbuf_t *cbuf, void *buf, size_t buf_len);
size_t circbuf_read(circbuf_t *cbuf, void *buf, size_t buf_len);
size_t circbuf_input_some(const circbuf_t *cbuf, const void **buf,
                          size_t buf_len);
size_t circbuf_dispose_some(circbuf_t *cbuf, size_t len);
/* TODO:
size_t circbuf_dispose_all(circbuf_t *cbuf);
int circbuf_pause_write(circbuf_t *cbuf);
int circbuf_unpause_write(circbuf_t *cbuf);
size_t circbuf_input(const circbuf_t *cbuf, const void **buf, size_t buf_len,
                     const void **buf2, size_t buf_len2);
size_t circbuf_dispose(circbuf_t *cbuf, size_t len);
*/

size_t circbuf_write_some(circbuf_t *cbuf, const void *buf, size_t buf_len);
size_t circbuf_write(circbuf_t *cbuf, const void *buf, size_t buf_len);
size_t circbuf_write_some2(circbuf_t *cbuf, circbuf_write_cb_t writer,
                           void *context, size_t write_len, char *eof_reached);
size_t circbuf_write2(circbuf_t *cbuf, circbuf_write_cb_t writer, void *context,
                      size_t write_len);

int circbuf_close_read(circbuf_t *cbuf);
int circbuf_close_write(circbuf_t *cbuf);

#endif /* CIRCBUF_H */
