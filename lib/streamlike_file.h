#ifndef STREAMLIKE_FILE_H
#define STREAMLIKE_FILE_H

#include <stdio.h>

#include "streamlike.h"

size_t sl_fread(void *context, void *buffer, size_t size);

size_t sl_fwrite(void *context, const void *buffer, size_t size);

int sl_fseek(void *context, off_t offset, int whence);

off_t sl_ftell(void *context);

int sl_feof(void *context);

int sl_ferror(void *context);

off_t sl_flength(void *context);

sl_seekable_t sl_fseekable(void *context);

streamlike_t* sl_fopen(const char *path, const char *mode);

streamlike_t* sl_fopen2(FILE *file);

int sl_fclose(streamlike_t *stream);

#endif /* STREAMLIKE_FILE_H */
