#ifdef SL_DEBUG
#include "debug.h"
#endif

#ifndef SL_FILE_ASSERT
# ifdef SL_ASSERT
#  define SL_FILE_ASSERT(...) SL_ASSERT(__VA_ARGS__)
# else
#  define SL_FILE_ASSERT(...) ((void)0)
# endif
#endif
#include "file.h"

#include <stdlib.h>
#include <sys/stat.h>

streamlike_t* sl_fopen(const char *path, const char *mode)
{
    FILE *file;

    SL_FILE_ASSERT(path != NULL);
    SL_FILE_ASSERT(mode != NULL);

    file = fopen(path, mode);
    if (!file) {
        return NULL;
    }
    return sl_fopen2(file);
}

streamlike_t* sl_fopen2(FILE *file)
{
    streamlike_t *stream;

    SL_FILE_ASSERT(file != NULL);

    stream = malloc(sizeof(streamlike_t));
    if (!stream) {
        return NULL;
    }

    stream->context = file;
    stream->read    = sl_fread_cb;
    stream->input   = NULL;
    stream->write   = sl_fwrite_cb;
    stream->flush   = sl_fflush_cb;
    stream->seek    = sl_fseek_cb;
    stream->tell    = sl_ftell_cb;
    stream->eof     = sl_feof_cb;
    stream->error   = sl_ferror_cb;
    stream->length  = sl_flength_cb;

    stream->seekable     = sl_fseekable_cb;
    stream->ckp_count    = NULL;
    stream->ckp          = NULL;
    stream->ckp_offset   = NULL;
    stream->ckp_metadata = NULL;

    return stream;
}

int sl_fclose(streamlike_t *stream)
{
    SL_FILE_ASSERT(stream != NULL);
    SL_FILE_ASSERT(stream->context != NULL);

    if (fclose((FILE*)stream->context) < 0) {
        return -1;
    }
    free(stream);

    return 0;
}

size_t sl_fread_cb(void *context, void *buffer, size_t size)
{
    return fread(buffer, 1, size, (FILE*)context);
}

size_t sl_fwrite_cb(void *context, const void *buffer, size_t size)
{
    return fwrite(buffer, 1, size, (FILE*)context);
}

int sl_fflush_cb(void *context)
{
    return fflush((FILE*)context);
}

int sl_fseek_cb(void *context, off_t offset, int whence)
{
    switch(whence)
    {
        case SL_SEEK_SET:
            return fseek((FILE*)context, offset, SEEK_SET);
        case SL_SEEK_CUR:
            return fseek((FILE*)context, offset, SEEK_CUR);
        case SL_SEEK_END:
            return fseek((FILE*)context, offset, SEEK_END);
    }
    return fseek((FILE*)context, offset, whence);
}

off_t sl_ftell_cb(void *context)
{
    return ftello((FILE*)context);
}

int sl_feof_cb(void *context)
{
    return feof((FILE*)context);
}

int sl_ferror_cb(void *context)
{
    return ferror((FILE*)context);
}

off_t sl_flength_cb(void *context)
{
    struct stat s;
    int fd = fileno((FILE*)context);

    if (fd < 0) {
        return -1;
    }
    if (fstat(fd, &s) < 0) {
        return -2;
    }
    return s.st_size;
}

sl_seekable_t sl_fseekable_cb(void *context)
{
    return SL_SEEKING_SUPPORTED;
}
