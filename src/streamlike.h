/**
 * \file
 * Streamlike API header.
 *
 * This library provides an abstract way of accessing to underlying resource
 * stream (file, url etc.) along with a high-level random-access signaling
 * capacity.
 */
#ifndef STREAMLIKE_H
#define STREAMLIKE_H

#ifdef SL_DEBUG
#include "streamlike/debug.h"
#endif

#ifndef SL_ASSERT
#define SL_ASSERT(x) ((void)0)
#endif

/**
 * \defgroup MacroDefinitions Library Macros
 *
 * @{
 */

/* For off_t and size_t. */
#include <sys/types.h>

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
 *
 * @{
 */
#define SL_SEEK_SET (0)
#define SL_SEEK_CUR (1)
#define SL_SEEK_END (2)
/** @} */ // Seek Whence Definitions
/** @} */ // MacroDefinitions

/**
 * \defgroup DataStructures Library Data Structures
 *
 * @{
 */

/**
 * Enumeration to denote seekability.
 */
typedef enum sl_seekable_e
{
    SL_SEEKING_NOT_SUPPORTED = 0, /**< Seeking isn't supported at all (Value:
                                    `0`). */
    SL_SEEKING_SUPPORTED     = 1, /**< Seeking is supported completely (Value:
                                    `1`). */
    SL_SEEKING_EMULATED      = 2, /**< Seeking is emulated through reading and
                                    discarding data read (Value: `2`). */
    SL_SEEKING_CHECKPOINTS   = 3  /**< Seeking to checkpoints is supported,
                                    while seeking to other parts is emulated
                                    (Value: `3`). */
} sl_seekable_t;

/**
 * Opaque definition for passing checkpoints around callbacks.
 *
 * This type will never be defined. Callbacks using pointer of this type should
 * cast it back and forth between their internal data structure to use it.
 */
typedef struct sl_ckp_s sl_ckp_t;

/** @} */ // DataStructures

/**
 * \defgroup Callbacks Callback Definitions
 *
 * Generic callback interface to access/modify underlying stream.
 *
 * @{
 */

/**
 * \name Basic Access Callback Definitions
 *
 * @{
 */

/**
 * Callback type to read from a stream. This function behaves like
 * `fread(buffer, 1, bytes, context)`.
 *
 * \param context Pointer to user-defined stream data.
 * \param buffer  Buffer to read into.
 * \param size    Unsigned number of bytes to read.
 *
 * \return Unsigned number of bytes successfully read. If less than \p size,
 *         this means end-of-file reached or there is some error.
 *
 * \see sl_read(), sl_eof_cb_t(), sl_error_cb_t(), sl_input_cb_t(),
 *      sl_write_cb_t()
 */
typedef
size_t (*sl_read_cb_t)(void *context, void *buffer, size_t size);

/**
 * Callback type to set input buffers.
 *
 * An alternative to sl_read_cb_t() to avoid extra copy. Buffer can be pointed
 * to the output buffer of underlying stream to avoid copying it again to
 * another buffer while reading as done in sl_read_cb_t().
 *
 * \param context Pointer to user-defined stream data.
 * \param buffer  Pointer to buffer to set.
 * \param size    Unsigned number of bytes to read.
 *
 * \return Unsigned number of bytes pointed by the buffer. If less than \p size,
 *         this means end-of-file reached or there is some error.
 *
 * \see sl_input(), sl_eof_cb_t(), sl_error_cb_t(), sl_read_cb_t(),
 *      sl_write_cb_t(),
 */
typedef
size_t (*sl_input_cb_t)(void *context, const void **buffer, size_t size);

/**
 * Callback type to write to a stream. This function behaves like
 * `fwrite(buffer, 1, bytes, context)`.
 *
 * \param context Pointer to user-defined stream data.
 * \param buffer  Buffer to write from.
 * \param size    Unsigned number of bytes to write.
 *
 * \return Unsigned number of bytes successfully written. Less than \p size on
 *         error.
 *
 * \see sl_write(), sl_flush_cb_t(), sl_eof_cb_t(), sl_error_cb_t(),
 *      sl_read_cb_t(), sl_input_cb_t()
 */
typedef
size_t (*sl_write_cb_t)(void *context, const void *buffer, size_t size);

/**
 * Callback type to flush remaining written data to stream.
 *
 * \param context Pointer to user-defined stream data.
 *
 * \return Zero on succes. Negative value on error.
 *
 * \see sl_flush(), sl_write_cb_t()
 */
typedef
int (*sl_flush_cb_t)(void *context);

/**
 * Callback type to seek to an offset in a stream. This function
 *
 * \param context Pointer to user-defined stream data.
 * \param offset  Number of bytes relative to the \p whence.
 * \param whence  Position used as reference for the \p offset.
 *
 * \return Zero on success. Nonzero otherwise.
 *
 * \see sl_seek(), sl_tell_cb_t(), sl_seekable_cb_t()
 */
typedef
int (*sl_seek_cb_t)(void *context, off_t offset, int whence);

/**
 * Callback type to get current offset in a stream.
 *
 * \param context Pointer to user-defined stream data.
 *
 * \return Current offset in the stream. Negative value on error.
 *
 * \see sl_tell(), sl_seek_cb_t(), sl_seekable_cb_t()
 */
typedef
off_t (*sl_tell_cb_t)(void *context);

/**
 * Callback type to check if end-of-file is reached in a stream.
 *
 * \param context Pointer to user-defined stream data.
 *
 * \return Nonzero value if end-of-file is reached.
 *
 * \see sl_eof(), sl_error_cb_t(), sl_read_cb_t(), sl_read_input_cb_t()
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
 *
 * \see sl_error(), sl_eof_cb_t(), sl_read_cb_t(), sl_read_input_cb_t()
 */
typedef
int (*sl_error_cb_t)(void *context);

/**
 * Callback type to get length of a stream.
 *
 * \param context Pointer to user-defined stream data.
 *
 * \return Length of the stream. Negative value on error.
 *
 * \see sl_length()
 */
typedef
off_t (*sl_length_cb_t)(void *context);

/** @} */ // Basic Access Callback Definitions

/**
 * \name Random-Access Callback Definitions
 *
 * Callbacks providing access to random access capabilities. This interface
 * allows an application to take advantage of random-access facilities provided
 * by underlying input stream. It is useful in particular if underlying input
 * source has some limitations in seeking such as emulated seek or
 * checkpoint-based seek in the data. Application can optimize seeking requests
 * with respect to these limitations and feedback received from the input
 * source.
 *
 * @{
 */

/**
 * Callback type to query seeking capability of a stream.
 *
 * \param context Pointer to user-defined stream data.
 *
 * \return Enum value denoting seeking capability.
 *
 * \see sl_seekable(), sl_seek_cb_t(), sl_ckp_offset_t()
 */
typedef
sl_seekable_t (*sl_seekable_cb_t)(void *context);

/**
 * Callback type to query checkpoint count in a stream.
 *
 * \param context Pointer to user-defined stream data.
 *
 * \return Number of checkpoints in the stream. Negative value if not supported.
 *
 * \see sl_ckp_count(), sl_seekable_cb_t(), sl_ckp_cb_t(), sl_ckp_offset_cb_t(),
 *      sl_ckp_metadata_cb_t()
 */
typedef
int (*sl_ckp_count_cb_t)(void *context);

/**
 * Callback type to get a checkpoint from its index.
 *
 * \param context Pointer to user-defined stream data.
 * \param idx     Indice of the checkpoint
 *
 * \return Pointer to checkpoint. `NULL` if indice is out-of-range or checkpoint
 *         is not supported.
 *
 * \see sl_ckp(), sl_seekable_cb_t(), sl_ckp_cb_t(), sl_ckp_offset_cb_t(),
 *      sl_ckp_metadata_cb_t()
 */
typedef
const sl_ckp_t* (*sl_ckp_cb_t)(void *context, int idx);

/**
 * Callback type to get offset of a checkpoint.
 *
 * \param context Pointer to user-defined stream data.
 * \param ckp     Pointer to checkpoint
 *
 * \return Offset stored in checkpoint that can be used as `offset` parameter in
 *         #sl_seek_cb_t().
 *
 * \see sl_ckp_offset(), sl_seekable_cb_t(), sl_ckp_cb_t(), sl_ckp_count_cb_t(),
 *      sl_ckp_metadata_cb_t()
 */
typedef
off_t (*sl_ckp_offset_cb_t)(void *context, const sl_ckp_t* ckp);

/**
 * Callback type to get metadata of a checkpoint.
 *
 * \param context Pointer to user-defined stream data.
 * \param ckp     Pointer to checkpoint
 * \param result  Output pointer to store the pointer to read-only metadata.
 *
 * \return Unsigned length of the metadata.
 *
 * \see sl_ckp_offset(), sl_seekable_cb_t(), sl_ckp_cb_t(), sl_ckp_count_cb_t(),
 *      sl_ckp_metadata_cb_t()
 */
typedef
size_t (*sl_ckp_metadata_cb_t)(void *context, const sl_ckp_t* ckp,
                               const void** result);

/** @} */ // Random-Access Callback Definitions
/** @} */ // Callbacks

/**
 * \ingroup DataStructures
 *
 * Layout for a stream including all callbacks.
 *
 * This struct abstracts I/O so that it can be customized easily. If a function
 * callback is `NULL`, it means this operation is not supported. However, the
 * opposite may not be true: If a function is not `NULL`, it is not guaranteed
 * that the operation is supported.
 */
typedef struct streamlike_s
{
    void *context;         /**< Stream context to pass around callbacks. */
    sl_read_cb_t   read;   /**< Read from the stream. */
    sl_input_cb_t  input;  /**< Read from the stream through output pointer. */
    sl_write_cb_t  write;  /**< Write to the stream. */
    sl_flush_cb_t  flush;  /**< Flush to the stream. */
    sl_seek_cb_t   seek;   /**< Seek to offset in the stream. */
    sl_tell_cb_t   tell;   /**< Tell current offset of the stream. */
    sl_eof_cb_t    eof;    /**< Check if end-of-file reached. */
    sl_error_cb_t  error;  /**< Check if an error happened. */
    sl_length_cb_t length; /**< Get length of the stream. */

    /* Random access and checkpoint details. */
    sl_seekable_cb_t  seekable;  /**< Get seeking capability of the stream. */
    sl_ckp_count_cb_t ckp_count; /**< Get checkpoints count in the stream. */
    sl_ckp_cb_t       ckp;       /**< Get a checkpoint from the stream. */
    sl_ckp_offset_cb_t   ckp_offset;   /**< Get offset of the checkpoint. */
    sl_ckp_metadata_cb_t ckp_metadata; /**< Get metadata of the checkpoint. */
} streamlike_t;

/**
 * \defgroup WrapperFunctions Wrapper Functions
 *
 * Wrapper functions provided to short hand cumbersome callbacks.
 *
 * @{
 */

/**
 * \name Basic Access Wrapper Functions
 *
 * Short hand functions provided for convenience to use basic access callbacks.
 *
 * \note This functions will cause segmentation fault if provided `stream` or
 * the requested capability is `NULL`. Compiling with `SL_DEBUG` will activate
 * necessary assertions. See SL_ASSERT() for details about assertions.
 *
 * @{
 */

/**
 * Wraps reading callback of a streamlike object.
 *
 * \see sl_read_cb_t()
 */
inline size_t sl_read(const streamlike_t *stream, void *buffer, size_t size)
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
inline size_t sl_input(const streamlike_t *stream, const void **buffer,
                       size_t size)
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
inline size_t sl_write(const streamlike_t *stream, const void *buffer,
                       size_t size)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->write);
    return stream->write(stream->context, buffer, size);
}

/**
 * Wraps flushing callback of a streamlike object.
 *
 * \see sl_flush_cb_t()
 */
inline int sl_flush(const streamlike_t *stream)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->flush);
    return stream->flush(stream->context);
}

/**
 * Wraps seeking callback of a streamlike object.
 *
 * \see sl_seek_cb_t()
 */
inline int sl_seek(const streamlike_t *stream, off_t offset, int whence)
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
inline off_t sl_tell(const streamlike_t *stream)
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
inline off_t sl_length(const streamlike_t *stream)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->length);
    return stream->length(stream->context);
}

/** @} */ // Basic Access Wrapper Functions

/**
 * \name Random-Access Wrapper Functions
 *
 * Short hand functions provided for convenience to use high-level random-access
 * streamlike callbacks.
 *
 * \note This functions will cause segmentation fault if provided `stream` or
 * the requested capability is `NULL`. Compiling with `SL_DEBUG` will activate
 * necessary assertions.
 *
 * @{
 */

/**
 * Wraps seekable callback of a streamlike object.
 *
 * \see sl_seekable_cb_t()
 */
inline sl_seekable_t sl_seekable(const streamlike_t *stream)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->seekable);
    return stream->seekable(stream->context);
}

/**
 * Wraps checkpoint count callback of a streamlike object.
 *
 * \see sl_ckp_count_cb_t()
 */
inline int sl_ckp_count(const streamlike_t *stream)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->ckp_count);
    return stream->ckp_count(stream->context);
}

/**
 * Wraps checkpoint getter callback of a streamlike object.
 *
 * \see sl_ckp_cb_t()
 */
inline const sl_ckp_t* sl_ckp(const streamlike_t *stream, int idx)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->ckp);
    return stream->ckp(stream->context, idx);
}

/**
 * Wraps checkpoint offset callback of a streamlike object.
 *
 * \see sl_ckp_offset_cb_t()
 */
inline off_t sl_ckp_offset(const streamlike_t *stream, const sl_ckp_t* ckp)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->ckp_offset);
    return stream->ckp_offset(stream->context, ckp);
}

/**
 * Wraps checkpoint metadata callback of a streamlike object.
 *
 * \see sl_ckp_metadata_cb_t()
 */
inline size_t sl_ckp_metadata(const streamlike_t *stream,
                              const sl_ckp_t* ckp, const void** result)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->ckp_metadata);
    return stream->ckp_metadata(stream->context, ckp, result);
}

/**
 * Convenience function to seek to a checkpoint. This function is a short hand
 * around sl_seek() and sl_ckp_offset(). It requires these callbacks to be
 * valid (i.e. they should have non-`NULL` values).
 *
 * \param stream Streamlike stream.
 * \param ckp    Checkpoint to seek.
 *
 * \return Return value of underlying sl_seek_cb_t() call.
 */
inline int sl_seek_to_ckp(const streamlike_t *stream, const sl_ckp_t* ckp)
{
    SL_ASSERT(stream);
    SL_ASSERT(stream->seek);
    SL_ASSERT(stream->ckp_offset);
    return sl_seek(stream, sl_ckp_offset(stream, ckp), SL_SEEK_SET);
}

/** @} */ // Random-Access Wrapper Functions
/** @} */ // Wrapper Functions

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* STREAMLIKE_H */
