/**
 * \file
 * Streamlike API header.
 *
 * This library provides an abstract way of accessing to underlying resource
 * stream (file, url etc.).
 */
#ifndef STREAMLIKE_H
#define STREAMLIKE_H

/**
 * Asertion is enabled if `SL_DEBUG` is defined. Standard C library assertion is
 * used when enabled.
 */
#if defined(SL_DEBUG)
# include <assert.h>
# define SL_ASSERT(x) assert(x)
#else
# define SL_ASSERT(x) while(0)
#endif

/* Include related libraries if size_t or off_t is enabled. */
#if defined(SL_USE_SIZE_T) || defined(SL_USE_OFF_T)
# include <sys/types.h>
#else
# include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \name Seek Whence Definitions
 *
 * Definitions used for seeking whence. These are similar to `SEEK_SET`,
 * `SEEK_CUR` and `SEEK_END` defined by the standard C library.
 *
 * \see sl_seek_cb_t()
 * @{
 */
#define SL_SEEK_SET (0)
#define SL_SEEK_CUR (1)
#define SL_SEEK_END (2)
/** @} */

/**
 * Defined as uint64_t by default. If `SL_USE_SIZE_T` macro is defined, this
 * will be defined as `size_t`.
 */
#ifdef SL_USE_SIZE_T
typedef size_t sl_size_t;
#else
typedef uint64_t sl_size_t;
#endif

/**
 * Defined as int64_t as default.  If `SL_USE_OFF_T` macro is defined, this will
 * be defined as `size_t`.
 */
#ifdef SL_USE_OFF_T
typedef off_t sl_off_t;
#else
typedef int64_t sl_off_t;
#endif

/**
 * Enumeration to denote seekability.
 */
typedef enum sl_seekable_e
{
    SL_SEEKING_NOT_SUPPORTED = 0, /**< Seeking isn't supported at all. */
    SL_SEEKING_SUPPORTED = 1,     /**< Seeking is supported completely. */
    SL_SEEKING_EMULATED = 2,      /**< Seeking is emulated through reading and
                                    discarding data read. */
    SL_SEEKING_CHECKPOINTS = 3    /**< Seeking to checkpoints is supported,
                                    while seeking to other parts is emulated. */
} sl_seekable_t;

/**
 * \name Callback Definitions
 *
 * @{
 */

/**
 * Callback type to read from a stream. This function behaves like
 * `fread(buffer, 1, bytes, context)`.
 *
 * \param context Pointer to user-defined stream data.
 * \param buffer  Buffer to read into.
 * \param size    Number of bytes to read.
 *
 * \return Number of bytes successfully read. If less than \p size, this means
 *         end-of-file reached or there is some error.
 *
 * \see sl_eof_cb_t(), sl_error_cb_t(), sl_input_cb_t(), sl_write_cb_t()
 */
typedef
sl_size_t (*sl_read_cb_t)(void *context, void *buffer, sl_size_t size);

/**
 * Callback type to set input buffers.
 *
 * An alternative to sl_read_cb_t() to avoid extra copy. Buffer can be pointed
 * to the output buffer of underlying stream to avoid copying it again to
 * another buffer while reading as done in sl_read_cb_t().
 *
 * \param context Pointer to user-defined stream data.
 * \param buffer  Pointer to buffer to set.
 * \param size    Number of bytes to read.
 *
 * \return Number of bytes pointed by the buffer. If less than \p size, this
 *         means end-of-file reached or there is some error.
 *
 * \see sl_eof_cb_t(), sl_error_cb_t(), sl_read_cb_t(), sl_write_cb_t(),
 */
typedef
sl_size_t (*sl_input_cb_t)(void *context, const void **buffer, sl_size_t size);

/**
 * Callback type to write to a stream. This function behaves like
 * `fwrite(buffer, 1, bytes, context)`.
 *
 * \param context Pointer to user-defined stream data.
 * \param buffer  Buffer to write from.
 * \param size    Number of bytes to write.
 *
 * \return Number of bytes successfully written. Less than \p size on error.
 */
typedef
sl_size_t (*sl_write_cb_t)(void *context, const void *buffer, sl_size_t size);

/**
 * Callback type to seek to an offset in a stream. This function
 *
 * \param context Pointer to user-defined stream data.
 * \param offset  Number of bytes relative to the \p whence.
 * \param whence  Position used as reference for the \p offset.
 *
 * \return Zero on success.
 * \return Nonzero otherwise.
 */
typedef
int (*sl_seek_cb_t)(void *context, sl_off_t offset, int whence);

/**
 * Callback type to get current offset in a stream.
 *
 * \param context Pointer to user-defined stream data.
 *
 * \return Current offset in the stream.
 * \return Negative value on error.
 */
typedef
sl_off_t (*sl_tell_cb_t)(void *context);

/**
 * Callback type to check if end-of-file is reached in a stream.
 *
 * \param context Pointer to user-defined stream data.
 *
 * \return Nonzero value if end-of-file is reached.
 */
typedef
int (*sl_eof_cb_t)(void *context);

/**
 * Callback type to check if any errors happened while reading from/writing to a
 * stream.
 *
 * This function will be used to check and return specific error in case of
 * sl_read_cb_t() or sl_write_cb_t() fail to read or write some part of data.
 * Note that this method is not used for checking errors for stream API calls
 * which can return negative value themselves to indicate errors.
 *
 * \param context Pointer to user-defined stream data.
 *
 * \return Nonzero value if there had been an error in previous read/write call.
 */
typedef
int (*sl_error_cb_t)(void *context);

/**
 * Callback type to get length of a stream.
 *
 * \param context Pointer to user-defined stream data.
 *
 * \return Length of the stream.
 * \return Negative value on error:
 *         - `-1` is reserved for the case where the stream is continuous.
 *         **Not implemented yet.**
 *
 * \todo Implement continuous streams. Either remove dependency to this
 *       function, or make it optional.
 */
typedef
sl_off_t (*sl_length_cb_t)(void *context);

/** @} */

/**
 * Layout for a stream.
 *
 * This struct abstracts I/O so that it can be customized easily. If a function
 * callback is NULL, it means this operation is not supported. However, the
 * opposite may not be true: If a function is not NULL, it is not guaranteed
 * that the operation is supported.
 */
typedef struct streamlike_s
{
    void *context;
    sl_read_cb_t   read;
    sl_input_cb_t  input;
    sl_write_cb_t  write;
    sl_seek_cb_t   seek;
    sl_tell_cb_t   tell;
    sl_eof_cb_t    eof;
    sl_error_cb_t  error;
    sl_length_cb_t length;
} streamlike_t;

/**
 * \name Wrapper Functions
 *
 * Short hand functions provided for convenience to use streamlike callbacks.
 *
 * \note This functions will cause segmentation fault if provided `stream` or
 * the requested capability is NULL. Compiling with `SL_DEBUG` will activate
 * assertions for checking if provided `stream` is NULL.
 *
 * @{
 */

/**
 * Wraps reading callback of a streamlike object.
 *
 * \see sl_read_cb_t()
 */
inline sl_size_t sl_read(const streamlike_t *stream, void *buffer, sl_size_t size)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->read);
    return stream->read(stream->context, buffer, size);
}

/**
 * Wraps input callback of a streamlike object.
 *
 * \see sl_input_cb_t()
 */
inline sl_size_t sl_input_t(const streamlike_t *stream, const void **buffer,
                            sl_size_t size)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->input);
    return stream->input(stream->context, buffer, size);
}

/**
 * Wraps writing callback of a streamlike object.
 *
 * \see sl_write_cb_t()
 */
inline sl_size_t sl_write(const streamlike_t *stream, const void *buffer,
                          sl_size_t size)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->write);
    return stream->write(stream->context, buffer, size);
}

/**
 * Wraps seeking callback of a streamlike object.
 *
 * \see sl_seek_cb_t()
 */
inline int sl_seek(const streamlike_t *stream, sl_off_t offset, int whence)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->seek);
    return stream->seek(stream->context, offset, whence);
}

/**
 * Wraps offset querying callback of a streamlike object.
 *
 * \see sl_tell_cb_t()
 */
inline sl_off_t sl_tell(const streamlike_t *stream)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->tell);
    return stream->tell(stream->context);
}

/**
 * Wraps end-of-file checking callback of a streamlike object.
 *
 * \see sl_eof_cb_t()
 */
inline int sl_eof(const streamlike_t *stream)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->eof);
    return stream->eof(stream->context);
}

/**
 * Wraps error checking callback of a streamlike object.
 *
 * \see sl_error_cb_t()
 */
inline int sl_error(const streamlike_t *stream)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->error);
    return stream->error(stream->context);
}

/**
 * Wraps length callback of a streamlike object.
 *
 * \see sl_length_cb_t()
 */
inline sl_off_t sl_length(const streamlike_t *stream)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->length);
    return stream->length(stream->context);
}

/** @} */

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* ZIDX_STREAM_H */
