#include <stdlib.h>
#include <string.h>
#include <check.h>
#include <pthread.h>
#include <unistd.h>

#include "circbuf.c"

#define BUFFER_SIZE (1024*1024)
#define DATA_SIZE (50*BUFFER_SIZE)

char data[DATA_SIZE];
char buf[BUFFER_SIZE];
const void *pbuf;
circbuf_t *cbuf;
int roffset;
int roffset_next;
int woffset;

int verify_read(size_t len)
{
    if(!memcmp(buf, data + roffset, len)) {
        return 1;
    }
    int i;
    for (i = 0; i < len; i++)
    {
        if (buf[i] != data[roffset + i]) {
            return 0;
        }
    }
    return 1;
}

int verify_input(size_t len)
{
    return !memcmp(pbuf, data + roffset, len);
}

size_t data_read(size_t len)
{
    size_t consumed;
    roffset = roffset_next;
    consumed = circbuf_read(cbuf, buf, len);
    roffset_next = roffset + consumed;
    return consumed;
}

size_t data_read_some(size_t len)
{
    size_t consumed;
    roffset = roffset_next;
    consumed = circbuf_read_some(cbuf, buf, len);
    roffset_next = roffset + consumed;
    return consumed;
}

size_t data_input_some(size_t len)
{
    size_t input;
    pbuf = NULL;
    roffset = roffset_next;
    input = circbuf_input_some(cbuf, &pbuf, len);
    ck_assert(pbuf != NULL || len == 0);
    return input;
}

size_t data_write(size_t len)
{
    size_t written;
    ck_assert(woffset + len <= DATA_SIZE);
    written = circbuf_write(cbuf, data + woffset, len);
    woffset += written;
    return written;
}

size_t data_dispose(size_t len)
{
    size_t disposed;
    disposed = circbuf_dispose_some(cbuf, len);
    roffset_next += disposed;
    return disposed;
}

void fill_random_data()
{
    int i;
    srand(0);
    for(i = 0; i < DATA_SIZE; i++) {
        data[i] = (char) rand();
    }
}

START_TEST(test_sequential)
{
    cbuf = circbuf_init(BUFFER_SIZE);
    roffset = 0;
    woffset = 0;

    ck_assert(data_write(50) == 50);
    ck_assert(data_read(50) == 50);
    ck_assert(verify_read(50));

    ck_assert(data_write(50) == 50);
    ck_assert(data_read_some(60) == 50);
    ck_assert(verify_read(50));
    ck_assert(data_read_some(60) == 0);

    ck_assert(data_write(50) == 50);
    ck_assert(data_dispose(30) == 30);
    ck_assert(data_read_some(10) == 10);
    ck_assert(verify_read(10));
    ck_assert(data_read_some(20) == 10);
    ck_assert(verify_read(10));

    ck_assert(data_write(50) == 50);
    ck_assert(data_input_some(50) == 50);
    ck_assert(verify_input(50));
    ck_assert(data_input_some(50) == 50);
    ck_assert(verify_input(50));
    ck_assert(data_input_some(60) == 50);
    ck_assert(verify_input(50));
    ck_assert(data_dispose(30) == 30);
    ck_assert(data_dispose(30) == 20);
    ck_assert(data_input_some(60) == 0);
    ck_assert(data_read_some(60) == 0);

    ck_assert(data_write(50) == 50);
    ck_assert(data_read_some(60) == 50);
    ck_assert(verify_read(50));
    ck_assert(data_read_some(60) == 0);

    circbuf_destroy(cbuf);
}
END_TEST

START_TEST(test_sequential_fill)
{
    size_t whole_buffer_size;
    cbuf = circbuf_init(BUFFER_SIZE);
    roffset = 0;
    woffset = 0;

    whole_buffer_size = ((circbuf_t_*)cbuf)->size;

    ck_assert(data_write(whole_buffer_size) == whole_buffer_size);
    ck_assert(data_read(whole_buffer_size) == whole_buffer_size);
    ck_assert(verify_read(whole_buffer_size));
    ck_assert(data_read_some(whole_buffer_size) == 0);

    circbuf_destroy(cbuf);
}
END_TEST

START_TEST(test_sequential_read_around)
{
    size_t almost_until_end;
    size_t little_more;
    size_t some_more;

    cbuf = circbuf_init(BUFFER_SIZE);
    roffset = 0;
    woffset = 0;

    almost_until_end = ((circbuf_t_*)cbuf)->size - 5;
    little_more = 3;
    some_more = 7;

    ck_assert(data_write(almost_until_end) == almost_until_end);
    ck_assert(data_read(almost_until_end) == almost_until_end);
    ck_assert(verify_read(almost_until_end));
    ck_assert(data_read_some(almost_until_end) == 0);

    ck_assert(data_write(little_more + some_more) == little_more + some_more);
    ck_assert(data_read(little_more) == little_more);
    ck_assert(verify_read(little_more));
    ck_assert(data_read(some_more) == some_more);
    ck_assert(verify_read(some_more));
    ck_assert(data_read_some(some_more) == 0);

    circbuf_destroy(cbuf);
}
END_TEST

START_TEST(test_sequential_dispose_around)
{
    size_t almost_until_end;
    size_t little_more;
    size_t some_more;

    cbuf = circbuf_init(BUFFER_SIZE);
    roffset = 0;
    woffset = 0;

    almost_until_end = ((circbuf_t_*)cbuf)->size - 5;
    little_more = 3;
    some_more = 7;

    ck_assert(data_write(almost_until_end) == almost_until_end);
    ck_assert(data_dispose(almost_until_end) == almost_until_end);
    ck_assert(data_read_some(almost_until_end) == 0);

    ck_assert(data_write(little_more + some_more) == little_more + some_more);
    ck_assert(data_dispose(little_more) == little_more);
    ck_assert(data_dispose(some_more) == some_more);
    ck_assert(data_read_some(some_more) == 0);

    ck_assert(data_write(little_more) == little_more);
    ck_assert(data_read(little_more) == little_more);
    ck_assert(verify_read(little_more));

    circbuf_destroy(cbuf);
}
END_TEST

START_TEST(test_sequential_input_around)
{
    size_t almost_until_end;
    size_t little_more;
    size_t some_more;
    size_t margin;
    size_t span;

    cbuf = circbuf_init(BUFFER_SIZE);
    roffset = 0;
    woffset = 0;

    margin = 5;
    span = 2;
    almost_until_end = ((circbuf_t_*)cbuf)->size - margin;
    little_more = margin - span;
    some_more = margin + span;

    ck_assert(data_write(almost_until_end) == almost_until_end);
    ck_assert(data_input_some(almost_until_end) == almost_until_end);
    ck_assert(verify_input(almost_until_end));
    ck_assert(data_dispose(almost_until_end) == almost_until_end);
    ck_assert(data_read_some(almost_until_end) == 0);

    ck_assert(data_write(little_more + some_more) == little_more + some_more);
    ck_assert(data_input_some(little_more) == little_more);
    ck_assert(verify_input(little_more));
    ck_assert(data_dispose(little_more) == little_more);

    ck_assert(data_input_some(some_more) == span);
    ck_assert(verify_input(span));
    ck_assert(data_dispose(span) == span);

    ck_assert(data_input_some(some_more - span) == some_more - span);
    ck_assert(verify_input(some_more - span));
    ck_assert(data_dispose(some_more - span) == some_more - span);
    ck_assert(data_read_some(some_more) == 0);

    ck_assert(data_write(little_more) == little_more);
    ck_assert(data_read(little_more) == little_more);
    ck_assert(verify_read(little_more));

    circbuf_destroy(cbuf);
}
END_TEST

void* serial_read(void* argument)
{
    int (*continue_callback)() = argument;
    size_t step;
    size_t read;
    while(1)
    {
        step = continue_callback();
        if (!step) {
            ck_assert(circbuf_close_read(cbuf) == 0);
            return NULL;
        }
        read = data_read(step);
        ck_assert(read == step || circbuf_is_write_closed(cbuf));
        ck_assert(verify_read(read));
    }
}

void* serial_write(void* argument)
{
    int (*continue_callback)() = argument;
    size_t step;
    size_t written;
    while(1)
    {
        step = continue_callback();
        if (!step) {
            ck_assert(circbuf_close_write(cbuf) == 0);
            return NULL;
        }
        written = data_write(step);
        ck_assert(written == step || circbuf_is_read_closed(cbuf));
    }
}

int normal_reader()
{
    int step = BUFFER_SIZE / 10;
    if (roffset + step <= DATA_SIZE) {
        return step;
    }
    return DATA_SIZE - roffset;
}

int normal_writer()
{
    int step = BUFFER_SIZE / 10;
    if (woffset + step <= DATA_SIZE) {
        return step;
    }
    return DATA_SIZE - woffset;
}

int slow_reader()
{
    usleep(50);
    return normal_reader();
}

int slow_writer()
{
    usleep(50);
    return normal_writer();
}

START_TEST(test_concurrent_normal)
{
    cbuf = circbuf_init(BUFFER_SIZE);

    roffset = 0;
    woffset = 0;

    pthread_t reader_thread;

    ck_assert(pthread_create(&reader_thread, NULL, serial_read, normal_reader)
                == 0);

    serial_write(normal_writer);

    ck_assert(pthread_join(reader_thread, NULL) == 0);

    circbuf_destroy(cbuf);
}
END_TEST

START_TEST(test_concurrent_slow_reader)
{

    cbuf = circbuf_init(BUFFER_SIZE);

    roffset = 0;
    woffset = 0;

    pthread_t reader_thread;

    ck_assert(pthread_create(&reader_thread, NULL, serial_read, slow_reader)
                == 0);

    serial_write(normal_writer);

    ck_assert(pthread_join(reader_thread, NULL) == 0);

    circbuf_destroy(cbuf);
}
END_TEST

START_TEST(test_concurrent_slow_writer)
{

    cbuf = circbuf_init(BUFFER_SIZE);

    roffset = 0;
    woffset = 0;

    pthread_t reader_thread;

    ck_assert(pthread_create(&reader_thread, NULL, serial_read, normal_reader)
                == 0);

    serial_write(slow_writer);

    ck_assert(pthread_join(reader_thread, NULL) == 0);

    circbuf_destroy(cbuf);
}
END_TEST

START_TEST(test_concurrent_slow_both)
{

    cbuf = circbuf_init(BUFFER_SIZE);

    roffset = 0;
    woffset = 0;

    pthread_t reader_thread;

    ck_assert(pthread_create(&reader_thread, NULL, serial_read, slow_reader)
                == 0);

    serial_write(slow_writer);

    ck_assert(pthread_join(reader_thread, NULL) == 0);

    circbuf_destroy(cbuf);
}
END_TEST

Suite* circbuf_suite()
{
    Suite *s;
    TCase *tc;

    s = suite_create("Circular Buffer");

    tc = tcase_create("Sequential");
    tcase_add_test(tc, test_sequential);
    tcase_add_test(tc, test_sequential_fill);
    tcase_add_test(tc, test_sequential_read_around);
    tcase_add_test(tc, test_sequential_dispose_around);
    tcase_add_test(tc, test_sequential_input_around);
    suite_add_tcase(s, tc);

    tc = tcase_create("Concurrent");
    tcase_add_test(tc, test_concurrent_normal);
    tcase_add_test(tc, test_concurrent_slow_reader);
    tcase_add_test(tc, test_concurrent_slow_writer);
    tcase_add_test(tc, test_concurrent_slow_both);
    suite_add_tcase(s, tc);

    return s;
}

int main(int argc, char **argv)
{
    SRunner *sr;
    int num_failed;

    fill_random_data();

    sr = srunner_create(circbuf_suite());

    srunner_run_all(sr, CK_ENV);

    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
