#ifndef STREAMLIKE_FILE_H
#define STREAMLIKE_FILE_H

#include <stdio.h>
#include "../streamlike.h"

streamlike_t* sl_fopen(const char *path, const char *mode);
streamlike_t* sl_fopen2(FILE *file);
int sl_fclose(streamlike_t *stream);
int sl_fclose2(streamlike_t *stream);

size_t sl_fread_cb(void *context, void *buffer, size_t size);
size_t sl_fwrite_cb(void *context, const void *buffer, size_t size);
int sl_fflush_cb(void *context);
int sl_fseek_cb(void *context, off_t offset, int whence);
off_t sl_ftell_cb(void *context);
int sl_feof_cb(void *context);
int sl_ferror_cb(void *context);
off_t sl_flength_cb(void *context);
sl_seekable_t sl_fseekable_cb(void *context);

#endif /* STREAMLIKE_FILE_H */
