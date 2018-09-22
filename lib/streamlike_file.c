#include "streamlike_file.h"

#include <stdlib.h>
#include <sys/stat.h>

size_t sl_fread(void *context, void *buffer, size_t size)
{
    return fread(buffer, 1, size, (FILE*)context);
}

size_t sl_fwrite(void *context, const void *buffer, size_t size)
{
    return fwrite(buffer, 1, size, (FILE*)context);
}

int sl_fseek(void *context, off_t offset, int whence)
{
    return fseeko((FILE*)context, offset, whence);
}

off_t sl_ftell(void *context)
{
    return ftello((FILE*)context);
}

int sl_feof(void *context)
{
    return feof((FILE*)context);
}

int sl_ferror(void *context)
{
    return ferror((FILE*)context);
}

off_t sl_flength(void *context)
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

sl_seekable_t sl_fseekable(void *context)
{
    return SL_SEEKING_SUPPORTED;
}

streamlike_t* sl_fopen(const char *path, const char *mode)
{
    FILE *file = fopen(path, mode);
    if (!file) {
        return NULL;
    }
    return sl_fopen2(file);
}

streamlike_t* sl_fopen2(FILE *file)
{
    streamlike_t *stream = malloc(sizeof(streamlike_t));

    stream->context = file;
    stream->read    = sl_fread;
    stream->input   = NULL;
    stream->write   = sl_fwrite;
    stream->seek    = sl_fseek;
    stream->tell    = sl_ftell;
    stream->eof     = sl_feof;
    stream->error   = sl_ferror;
    stream->length  = sl_flength;

    stream->seekable     = sl_fseekable;
    stream->ckp_count    = NULL;
    stream->ckp          = NULL;
    stream->ckp_offset   = NULL;
    stream->ckp_metadata = NULL;

    return stream;
}

int sl_fclose(streamlike_t *stream)
{
    SL_ASSERT(stream != NULL);
    SL_ASSERT(stream->context != NULL);

    if (fclose((FILE*)stream->context) < 0) {
        return -1;
    }
    free(stream);

    return 0;
}
