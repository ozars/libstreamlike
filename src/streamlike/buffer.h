#ifndef STREAMLIKE_BUFFER_H
#define STREAMLIKE_BUFFER_H

#include "../streamlike.h"
#define SL_BUFFER_DEFAULT_BUFFER_SIZE (1024 * 1024 * 1024)
#define SL_BUFFER_DEFAULT_STEP_SIZE   (16 * 1024)

streamlike_t* sl_buffer_create(streamlike_t* inner_stream);
streamlike_t* sl_buffer_create2(streamlike_t* inner_stream, size_t buffer_size,
                                size_t step_size);
int sl_buffer_destroy(streamlike_t *buffer_stream);

int sl_buffer_threaded_fill_buffer(streamlike_t *buffer_stream);
int sl_buffer_blocking_fill_buffer(streamlike_t *buffer_stream);
int sl_buffer_close_buffer(streamlike_t *buffer_stream);

size_t sl_buffer_read_cb(void *context, void *buffer, size_t len);
size_t sl_buffer_input_cb(void *context, const void **buffer, size_t size);
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

#endif /* STREAMLIKE_BUFFER_H */
