/**
 * \file
 * Streamlike HTTP API header.
 *
 * Accesses HTTP data using streamlike interface.
 */
#ifndef STREAMLIKE_HTTP_H
#define STREAMLIKE_HTTP_H

#include "../streamlike.h"

/**
 * \defgroup HttpFunction Streamlike HTTP Functions
 *
 * @{
 */

/**
 * Initializes HTTP library globally.
 *
 * This function needs to called once to initialize the library. It is called
 * during link time by default. This makes underlying global cURL library
 * initialization call in a thread-safe manner.
 *
 * This function is safe to be called multiple times.
 */
void sl_http_library_init() __attribute__((constructor));

/**
 * Cleans up HTTP library globally.
 *
 * This function calls underlying global cURL library cleanup call. This
 * functions isafe to be called multiple times. If the library isn't
 * initialized, it does nothing.
 */
void sl_http_library_cleanup();

/**
 * Opens a stream to access provided URI.
 *
 * \param stream Stream to store http data.
 * \param uri    URI to navigate.
 *
 * \return Zero on success.
 *
 * \see sl_http_close(), sl_http_create(), sl_http_destroy()
 */
int sl_http_open(streamlike_t *stream, const char *uri);

/**
 * Closes a given HTTP stream.
 *
 * It is undefined behavior if given stream isn't a stream initialized by either
 * sl_http_open() or created by sl_http_create(). This function doesn't free
 * stream structure itself.
 *
 * \param stream Stream to close.
 *
 * \return Zero on success.
 *
 * \see sl_http_open(), sl_http_create(), sl_http_destroy()
 */
int sl_http_close(streamlike_t *stream);

/**
 * Allocates a stream and open it to access the provided URI.
 *
 * \param uri URI to navigate.
 *
 * \return Pointer to stream.
 *
 * \see sl_http_destroy(), sl_http_open(), sl_http_close()
 */
streamlike_t* sl_http_create(const char *uri);

/**
 * Destroys all resources used by given stream.
 *
 * This function closes the stream first and then releases the stream structure
 * itself.
 *
 * \param stream Stream to destroy.
 *
 * \return Zero on success. Return value of sl_http_close() function if there
 * had been an error during closing stream.
 *
 * \see sl_http_create(), sl_http_open(), sl_http_close()
 */
int sl_http_destroy(streamlike_t *stream);

/** @} */ // Streamlike HTTP Functions

/**
 * \defgroup HttpCallbacks Streamlike HTTP Callbacks
 *
 * @{
 */

/**
 * Read callback.
 *
 * \see sl_read_cb_t
 */
size_t sl_http_read_cb(void *context, void *buffer, size_t len);

/**
 * Seek callback.
 *
 * \see sl_seek_cb_t
 */
int sl_http_seek_cb(void *context, off_t offset, int whence);

/**
 * Tell callback.
 *
 * \see sl_tell_cb_t
 */
off_t sl_http_tell_cb(void *context);

/**
 * End of file callback.
 *
 * \see sl_eof_cb_t
 */
int sl_http_eof_cb(void *context);

/**
 * Error callback.
 *
 * \see sl_error_cb_t
 */
int sl_http_error_cb(void *context);

/**
 * Length callback.
 *
 * \see sl_length_cb_t
 */
off_t sl_http_length_cb(void *context);

/**
 * Seekable callback.
 *
 * \see sl_seekable_cb_t
 */
sl_seekable_t sl_http_seekable_cb(void *context);

/** @} */ // Streamlike HTTP Callbacks

#endif
