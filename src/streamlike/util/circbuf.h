/**
 * \file
 * Circular buffer.
 *
 * A threaded circular buffer implementation for one producer and one consumer.
 */
#ifndef CIRCBUF_H
#define CIRCBUF_H
#include<stddef.h>

/**
 * Opaque type for circular buffer.
 */
typedef struct circbuf_s circbuf_t;

/**
 * Circular buffer write callback type.
 *
 * This callback is used by functions writing into the circular buffer. If the
 * number of bytes written to buf is not equal to buf_len, this will be
 * interpreted as an error by circular buffer. When this callback is provided as
 * a parameter to a library function, the function will be aware of total number
 * of bytes to be written through calling this callback multiple times.
 *
 * \param   context Callback context provided by caller function.
 * \param   buf     Pointer to the circular buffer to be written.
 * \param   buf_len Length of the data that can be stored in buf.
 *
 * \return  Number of bytes written to buf.
 */
typedef
size_t (*circbuf_write_cb_t)(void *context, void *buf, size_t buf_len);

/**
 * Initializes a circular buffer of given size.
 *
 * \param   buf_len Number of bytes to be stored in the circular buffer.
 *
 * \return  Pointer to newly created circular buffer. NULL if buf_len is zero or
 *          memory allocations fail.
 *
 * \see     circbuf_destroy()
 */
circbuf_t* circbuf_init(size_t buf_len);

/**
 * Releases all sources (including pointer itself) used by the circular buffer.
 *
 * \param   Pointer to the circular buffer.
 *
 * \see     circbuf_init()
 */
void circbuf_destroy(circbuf_t* cbuf);

/**
 * Resets circular buffer.
 *
 * This function effectively disposes all data in the buffer.
 *
 * It is undefined behavior to call this function if the producer is trying to
 * write some data to the circular buffer or the consumer is trying to read some
 * data from it concurrent with this call.
 *
 * \param   cbuf    Pointer to the circular buffer.
 *
 * \see     circbuf_init()
 */
void circbuf_reset(circbuf_t *cbuf);

/**
 * Gives the allocated size of buffer.
 *
 * This function returns size of the buffer allocated internally. It is greater
 * than the buffer length used for initializing the circular buffer.
 *
 * \param   cbuf    Pointer to the circular buffer.
 *
 * \return  Size of the allocated buffer.
 */
size_t circbuf_get_size(const circbuf_t* cbuf);

/**
 * Gives the length of data available for reading.
 *
 * The length returned by this function may not be reliable if the circular
 * buffer is being modified (read, write etc.) concurrently.
 *
 * \param   cbuf    Pointer to the circular buffer.
 *
 * \return  Length of the circular buffer.
 */
size_t circbuf_get_length(const circbuf_t* cbuf);

/**
 * Returns whether consumer closed reading.
 *
 * \param   cbuf    Pointer to the circular buffer.
 *
 * \return  Zero if read is not closed.
 */
int circbuf_is_read_closed(const circbuf_t* cbuf);

/**
 * Returns whether producer closed writing.
 *
 * \param   cbuf    Pointer to the circular buffer.
 *
 * \return  Zero if write is not closed.
 */
int circbuf_is_write_closed(const circbuf_t* cbuf);

/**
 * Reads from buffer at most buf_len bytes without blocking.
 *
 * Reads until whichever occurs first: buf is full (= buf_len bytes), right end
 * boundary of the internal buffer structure is reached (< buf_len bytes),
 * all available data is read (< buf_len bytes).
 *
 * \param   cbuf    Pointer to the circular buffer.
 * \param   buf     Pointer to the buffer for reading into.
 * \param   buf_len Length of available space in buf.
 *
 * \return  Number of bytes read into buf.
 *
 * \see     circbuf_read()
 */
size_t circbuf_read_some(circbuf_t *cbuf, void *buf, size_t buf_len);

/**
 * Reads buf_len bytes from the circular buffer with blocking if necessary.
 *
 * Reads until buf is full. Blocks if buffer cannot be completely filled until
 * some data is made available or writing is closed by the producer.
 *
 * \param   cbuf    Pointer to the circular buffer.
 * \param   buf     Pointer to the buffer for reading into.
 * \param   buf_len Length of space available in buf.
 *
 * \return  Number of bytes read.
 *
 * \see     circbuf_read_some()
 */
size_t circbuf_read(circbuf_t *cbuf, void *buf, size_t buf_len);

/**
 * Gets a pointer to the next data sequence up to given length.
 *
 * \deprecated Input feature will be removed or changed substantially.
 */
size_t circbuf_input_some(const circbuf_t *cbuf, const void **buf,
                          size_t buf_len);

/**
 * Disposes up to len bytes data without blocking.
 *
 * Reads until whichever occurs first: buf is full (disposed buf_len bytes),
 * right end boundary of the internal buffer structure is reached (disposed less
 * than buf_len bytes), all data available is disposed (likewise).
 *
 * \param   cbuf    Pointer to the circular buffer.
 * \param   len     Number of bytes to dispose.
 *
 * \return  Number of bytes disposed.
 *
 * \todo    A blocking variant of this function, e.g. circbuf_dispose, is to be
 *          implemented.
 */
size_t circbuf_dispose_some(circbuf_t *cbuf, size_t len);

/**
 * Writes some data up to buf_len bytes without blocking.
 *
 * Writes until whichever occurs first: buf is done (wrote buf_len bytes), right
 * end boundary of the internal buffer structure is reached (wrote less than
 * buf_len bytes), all data available is written (likewise).
 *
 * \param   cbuf    Pointer to the circular buffer.
 * \param   buf     Pointer to the buffer for writing from.
 * \param   buf_len Length of data available in buf.
 *
 * \return  Number of bytes written.
 *
 * \see     circbuf_write_some2(), circbuf_write(), circbuf_write2()
 */
size_t circbuf_write_some(circbuf_t *cbuf, const void *buf, size_t buf_len);

/**
 * Writes buf_len bytes to the circular buffer with blocking if necessary.
 *
 * Writes entire buf. Blocks until some space is made available or reading is
 * closed by the producer if there is no available space in the circular buffer.
 *
 * \param   cbuf    Pointer to the circular buffer.
 * \param   buf     Pointer to the buffer for writing from.
 * \param   buf_len Length of data available in buf.
 *
 * \return  Number of bytes written.
 *
 * \see     circbuf_write2(), circbuf_write_some(), circbuf_write_some2()
 */
size_t circbuf_write(circbuf_t *cbuf, const void *buf, size_t buf_len);

/**
 * Writes some data up to buf_len bytes without blocking by calling given
 * callback function.
 *
 * Writes by making multiple calls to writer until whichever occurs first: buf
 * is written (wrote buf_len bytes), right end boundary of the internal buffer
 * structure is reached (wrote less than buf_len bytes), all data available is
 * written (likewise), writer returns shorter than request number of bytes
 * (likewise, but also sets eof).
 *
 * \param   cbuf    Pointer to the circular buffer.
 * \param   writer  Callback function used for writing data.
 * \param   context Pointer passed to callback function.
 * \param   len     Length of data to be written.
 * \param   eof     Output flag set if writer returns less than requested
 *                  number of bytes. Ignored if it is NULL.
 *
 * \return  Number of bytes written.
 *
 * \see     circbuf_write_cb_t, circbuf_write_some(), circbuf_write2(),
 *          circbuf_write()
 */
size_t circbuf_write_some2(circbuf_t *cbuf, circbuf_write_cb_t writer,
                           void *context, size_t len, char *eof);

/**
 * Writes buf_len bytes to the circular buffer with blocking if necessary, by
 * calling given callback function.
 *
 * Writes entire len of data by making multiple calls to writer. Blocks until
 * some space is made available or reading is closed by the producer if there is
 * no available space in the circular buffer.
 *
 * \param   cbuf    Pointer to the circular buffer.
 * \param   writer  Callback function used for writing data.
 * \param   context Pointer passed to callback function.
 * \param   len     Length of data to be written.
 *
 * \return  Number of bytes written. Less than len iff writer returns a value
 *          shorter than requested number of bytes or reading is closed.
 *
 * \see     circbuf_write_cb_t, circbuf_write(), circbuf_write_some2(),
 *          circbuf_write_some()
 */
size_t circbuf_write2(circbuf_t *cbuf, circbuf_write_cb_t writer, void *context,
                      size_t len);

/**
 * Closes reading and signals the producer.
 *
 * \param   cbuf    Pointer to the circular buffer.
 *
 * \return  Zero on success. Error if already closed.
 */
int circbuf_close_read(circbuf_t *cbuf);

/**
 * Closes writing and signals the producer.
 *
 * \param   cbuf    Pointer to the circular buffer.
 *
 * \return  Zero on success. Error if already closed.
 */
int circbuf_close_write(circbuf_t *cbuf);

#endif /* CIRCBUF_H */
