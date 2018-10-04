#ifndef STREAMLIKE_HTTP_H
#define STREAMLIKE_HTTP_H

#include "streamlike.h"

void sl_http_library_init() __attribute__((constructor));
void sl_http_library_cleanup();
int sl_http_open(streamlike_t *stream, const char *uri);
int sl_http_close(streamlike_t *stream);
streamlike_t* sl_http_create(const char *uri);
int sl_http_destroy(streamlike_t *stream);

size_t sl_http_read_cb(void *context, void *buffer, size_t len);
/* TODO:
size_t sl_http_input_cb(void *context, const void **buffer, size_t len);
*/
int sl_http_seek_cb(void *context, off_t offset, int whence);
off_t sl_http_tell_cb(void *context);
int sl_http_eof_cb(void *context);
int sl_http_error_cb(void *context);
off_t sl_http_length_cb(void *context);
sl_seekable_t sl_http_seekable_cb(void *context);

#endif
