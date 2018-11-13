#include <stdlib.h>
#include <string.h>
#include <check.h>
#include <pthread.h>
#include <unistd.h>

#include "streamlike/util/circbuf.h"

#define BUFFER_SIZE (1024*1024)
#define DATA_SIZE (50*BUFFER_SIZE)

#define EARLY_CLOSE_THRESHOLD (DATA_SIZE/3)
#define SLOW_CONCURRENT_TEST_TIMEOUT (15)

char data[DATA_SIZE];
char *buf;
const void *pbuf;
circbuf_t *cbuf;
size_t roffset;
size_t roffset_next;
size_t woffset;
unsigned int seed_base;

#define ck_verify_read_(len) ck_assert_mem_eq(buf, data + roffset, len)
#define ck_verify_input_(len) ck_assert_mem_eq(pbuf, data + roffset, len)

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
    ck_assert_uint_le(woffset + len, DATA_SIZE);
    written = circbuf_write(cbuf, data + woffset, len);
    woffset += written;
    return written;
}

size_t data_write_cb(void *context, void *buf, size_t buf_len)
{
    memcpy(buf, data + woffset, buf_len);
    woffset += buf_len;
    return buf_len;
}

size_t data_write2(size_t len)
{
    size_t written;
    ck_assert_uint_le(woffset + len, DATA_SIZE);
    written = circbuf_write2(cbuf, data_write_cb, NULL, len);
    return written;
}

size_t data_dispose(size_t len)
{
    size_t disposed;
    disposed = circbuf_dispose_some(cbuf, len);
    roffset_next += disposed;
    return disposed;
}

void setup_random_data()
{
    int i;
    srand(0);
    for(i = 0; i < DATA_SIZE; i++) {
        data[i] = (char) rand();
    }
}

void setup_global()
{
    cbuf = circbuf_init(BUFFER_SIZE);
    ck_assert(cbuf);

    buf = malloc(circbuf_get_size(cbuf));
    ck_assert(cbuf);

    roffset = 0;
    roffset_next = 0;
    woffset = 0;
}

void teardown_global()
{
    circbuf_destroy(cbuf);
    free(buf);
}

START_TEST(test_sequential)
{
    ck_assert_uint_eq(data_write(50), 50);
    ck_assert_uint_eq(data_read(50), 50);
    ck_verify_read_(50);

    ck_assert_uint_eq(data_write(50), 50);
    ck_assert_uint_eq(data_read_some(60), 50);
    ck_verify_read_(50);
    ck_assert_uint_eq(data_read_some(60), 0);

    ck_assert_uint_eq(data_write(50), 50);
    ck_assert_uint_eq(data_dispose(30), 30);
    ck_assert_uint_eq(data_read_some(10), 10);
    ck_verify_read_(10);
    ck_assert_uint_eq(data_read_some(20), 10);
    ck_verify_read_(10);

    ck_assert_uint_eq(data_write(50), 50);
    ck_assert_uint_eq(data_input_some(50), 50);
    ck_verify_input_(50);
    ck_assert_uint_eq(data_input_some(50), 50);
    ck_verify_input_(50);
    ck_assert_uint_eq(data_input_some(60), 50);
    ck_verify_input_(50);
    ck_assert_uint_eq(data_dispose(30), 30);
    ck_assert_uint_eq(data_dispose(30), 20);
    ck_assert_uint_eq(data_input_some(60), 0);
    ck_assert_uint_eq(data_read_some(60), 0);

    ck_assert_uint_eq(data_write(50), 50);
    ck_assert_uint_eq(data_read_some(60), 50);
    ck_verify_read_(50);
    ck_assert_uint_eq(data_read_some(60), 0);
}
END_TEST

START_TEST(test_sequential_fill)
{
    size_t whole_buffer_size = BUFFER_SIZE;

    ck_assert_uint_eq(data_write(whole_buffer_size), whole_buffer_size);
    ck_assert_uint_eq(data_read(whole_buffer_size), whole_buffer_size);
    ck_verify_read_(whole_buffer_size);
    ck_assert_uint_eq(data_read_some(whole_buffer_size), 0);
}
END_TEST

START_TEST(test_sequential_read_around)
{
    size_t almost_until_end;
    size_t little_more;
    size_t some_more;

    almost_until_end = circbuf_get_size(cbuf) - 5;
    little_more = 3;
    some_more = 7;

    ck_assert_uint_eq(data_write(almost_until_end), almost_until_end);
    ck_assert_uint_eq(data_read(almost_until_end), almost_until_end);
    ck_verify_read_(almost_until_end);
    ck_assert_uint_eq(data_read_some(almost_until_end), 0);

    ck_assert_uint_eq(data_write(little_more + some_more),
                      little_more + some_more);
    ck_assert_uint_eq(data_read(little_more), little_more);
    ck_verify_read_(little_more);
    ck_assert_uint_eq(data_read(some_more), some_more);
    ck_verify_read_(some_more);
    ck_assert_uint_eq(data_read_some(some_more), 0);
}
END_TEST

START_TEST(test_sequential_dispose_around)
{
    size_t almost_until_end;
    size_t little_more;
    size_t some_more;

    almost_until_end = circbuf_get_size(cbuf) - 5;
    little_more = 3;
    some_more = 7;

    ck_assert_uint_eq(data_write(almost_until_end), almost_until_end);
    ck_assert_uint_eq(data_dispose(almost_until_end), almost_until_end);
    ck_assert_uint_eq(data_read_some(almost_until_end), 0);

    ck_assert_uint_eq(data_write(little_more + some_more),
                      little_more + some_more);
    ck_assert_uint_eq(data_dispose(little_more), little_more);
    ck_assert_uint_eq(data_dispose(some_more), some_more);
    ck_assert_uint_eq(data_read_some(some_more), 0);

    ck_assert_uint_eq(data_write(little_more), little_more);
    ck_assert_uint_eq(data_read(little_more), little_more);
    ck_verify_read_(little_more);
}
END_TEST

START_TEST(test_sequential_input_around)
{
    size_t almost_until_end;
    size_t little_more;
    size_t some_more;
    size_t margin;
    size_t span;

    margin = 5;
    span = 2;
    almost_until_end = circbuf_get_size(cbuf) - margin;
    little_more = margin - span;
    some_more = margin + span;

    ck_assert_uint_eq(data_write(almost_until_end), almost_until_end);
    ck_assert_uint_eq(data_input_some(almost_until_end), almost_until_end);
    ck_verify_input_(almost_until_end);
    ck_assert_uint_eq(data_dispose(almost_until_end), almost_until_end);
    ck_assert_uint_eq(data_read_some(almost_until_end), 0);

    ck_assert_uint_eq(data_write(little_more + some_more),
                      little_more + some_more);
    ck_assert_uint_eq(data_input_some(little_more), little_more);
    ck_verify_input_(little_more);
    ck_assert_uint_eq(data_dispose(little_more), little_more);

    ck_assert_uint_eq(data_input_some(some_more), span);
    ck_verify_input_(span);
    ck_assert_uint_eq(data_dispose(span), span);

    ck_assert_uint_eq(data_input_some(some_more - span), some_more - span);
    ck_verify_input_(some_more - span);
    ck_assert_uint_eq(data_dispose(some_more - span), some_more - span);
    ck_assert_uint_eq(data_read_some(some_more), 0);

    ck_assert_uint_eq(data_write(little_more), little_more);
    ck_assert_uint_eq(data_read(little_more), little_more);
    ck_verify_read_(little_more);
}
END_TEST

START_TEST(test_sequential_write2)
{
    ck_assert_uint_eq(data_write2(50), 50);
    ck_assert_uint_eq(data_read(50), 50);
    ck_verify_read_(50);

    ck_assert_uint_eq(data_write2(50), 50);
    ck_assert_uint_eq(data_read_some(60), 50);
    ck_verify_read_(50);
    ck_assert_uint_eq(data_read_some(60), 0);

    ck_assert_uint_eq(data_write2(50), 50);
    ck_assert_uint_eq(data_dispose(30), 30);
    ck_assert_uint_eq(data_read_some(10), 10);
    ck_verify_read_(10);
    ck_assert_uint_eq(data_read_some(20), 10);
    ck_verify_read_(10);

    ck_assert_uint_eq(data_write2(50), 50);
    ck_assert_uint_eq(data_input_some(50), 50);
    ck_verify_input_(50);
    ck_assert_uint_eq(data_input_some(50), 50);
    ck_verify_input_(50);
    ck_assert_uint_eq(data_input_some(60), 50);
    ck_verify_input_(50);
    ck_assert_uint_eq(data_dispose(30), 30);
    ck_assert_uint_eq(data_dispose(30), 20);
    ck_assert_uint_eq(data_input_some(60), 0);
    ck_assert_uint_eq(data_read_some(60), 0);

    ck_assert_uint_eq(data_write2(50), 50);
    ck_assert_uint_eq(data_read_some(60), 50);
    ck_verify_read_(50);
    ck_assert_uint_eq(data_read_some(60), 0);
}
END_TEST

START_TEST(test_sequential_fill_write2)
{
    size_t whole_buffer_size = BUFFER_SIZE;

    ck_assert_uint_eq(data_write2(whole_buffer_size), whole_buffer_size);
    ck_assert_uint_eq(data_read(whole_buffer_size), whole_buffer_size);
    ck_verify_read_(whole_buffer_size);
    ck_assert_uint_eq(data_read_some(whole_buffer_size), 0);
}
END_TEST

START_TEST(test_sequential_read_around_write2)
{

    size_t almost_until_end;
    size_t little_more;
    size_t some_more;

    almost_until_end = circbuf_get_size(cbuf) - 5;
    little_more = 3;
    some_more = 7;

    ck_assert_uint_eq(data_write2(almost_until_end), almost_until_end);
    ck_assert_uint_eq(data_read(almost_until_end), almost_until_end);
    ck_verify_read_(almost_until_end);
    ck_assert_uint_eq(data_read_some(almost_until_end), 0);

    ck_assert_uint_eq(data_write2(little_more + some_more),
                      little_more + some_more);
    ck_assert_uint_eq(data_read(little_more), little_more);
    ck_verify_read_(little_more);
    ck_assert_uint_eq(data_read(some_more), some_more);
    ck_verify_read_(some_more);
    ck_assert_uint_eq(data_read_some(some_more), 0);
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
        if (!step || (circbuf_get_length(cbuf) == 0
                        && circbuf_is_write_closed(cbuf))) {
            ck_assert(circbuf_close_read(cbuf) == 0);
            return NULL;
        }
        read = data_read(step);
        ck_assert(read == step || circbuf_is_write_closed(cbuf));
        ck_verify_read_(read);
    }
}

void* serial_input(void* argument)
{
    int (*continue_callback)() = argument;
    size_t step;
    size_t input;
    while(1)
    {
        step = continue_callback();
        if (!step || (circbuf_get_length(cbuf) == 0
                        && circbuf_is_write_closed(cbuf))) {
            ck_assert(circbuf_close_read(cbuf) == 0);
            return NULL;
        }
        input = data_input_some(step);
        ck_assert_uint_le(input, step);
        ck_verify_input_(input);
        ck_assert_uint_eq(data_dispose(input), input);
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
        if (!step || circbuf_is_read_closed(cbuf)) {
            ck_assert(circbuf_close_write(cbuf) == 0);
            return NULL;
        }
        written = data_write(step);
        ck_assert(written == step || circbuf_is_read_closed(cbuf));
    }
}

void* serial_write2(void* argument)
{
    int (*continue_callback)() = argument;
    size_t step;
    size_t written;
    while(1)
    {
        step = continue_callback();
        if (!step || circbuf_is_read_closed(cbuf)) {
            ck_assert(circbuf_close_write(cbuf) == 0);
            return NULL;
        }
        written = data_write2(step);
        ck_assert(written == step || circbuf_is_read_closed(cbuf));
    }
}

size_t normal_consumer_step()
{
    size_t step = BUFFER_SIZE / 10;
    if (roffset + step <= DATA_SIZE) {
        return step;
    }
    return DATA_SIZE - roffset;
}

size_t normal_producer_step()
{
    size_t step = BUFFER_SIZE / 10;
    if (woffset + step <= DATA_SIZE) {
        return step;
    }
    return DATA_SIZE - woffset;
}

size_t slow_consumer_step()
{
    usleep(50);
    return normal_consumer_step();
}

size_t slow_producer_step()
{
    usleep(50);
    return normal_producer_step();
}

size_t variable_consumer_step()
{
    size_t data_interval = BUFFER_SIZE / 7;
    size_t interval_count = 5;
    size_t interval = (roffset / data_interval) % interval_count;

    switch (interval) {
        case 0:
            usleep(50);
            break;
        case 1:
            usleep(500);
            break;
        case 2:
            usleep(5);
            break;
        case 3:
            usleep(250);
            break;
        case 4:
            break;
    }
    return normal_consumer_step();
}

size_t variable_producer_step()
{
    size_t data_interval = BUFFER_SIZE / 5;
    size_t interval_count = 5;
    size_t interval = (woffset / data_interval) % interval_count;

    switch (interval) {
        case 0:
            usleep(50);
            break;
        case 1:
            usleep(500);
            break;
        case 2:
            usleep(5);
            break;
        case 3:
            usleep(250);
            break;
        case 4:
            break;
    }
    return normal_producer_step();
}

size_t random_consumer_step()
{
    static unsigned int current_seed_base = 0;
    static unsigned int seed = 0;
    if (current_seed_base != seed_base) {
        current_seed_base = seed_base;
        seed = current_seed_base;
    }
    usleep(rand_r(&seed) % 1000);
    return normal_consumer_step();
}

size_t random_producer_step()
{
    static unsigned int current_seed_base = 1;
    static unsigned int seed = 1;
    if (current_seed_base != seed_base + 1) {
        current_seed_base = seed_base + 1;
        seed = current_seed_base;
    }
    usleep(rand_r(&seed) % 1000);
    return normal_producer_step();
}

size_t early_close_consumer_step()
{
    size_t step = normal_consumer_step();
    size_t threshold = EARLY_CLOSE_THRESHOLD;
    if (roffset_next + step < threshold) {
        return step;
    }
    return threshold - roffset_next;
}

size_t early_close_producer_step()
{
    size_t step = normal_producer_step();
    size_t threshold = EARLY_CLOSE_THRESHOLD;
    if (woffset + step < threshold) {
        return step;
    }
    return threshold - woffset;
}

#define CONCURRENT_TEST2(tname, consumer_thread_main, producer_thread_main, \
                         consumer_callback, producer_callback, initialize, \
                         bytes_count, ...) \
    START_TEST(tname) \
    { \
        (void)(initialize); \
        \
        pthread_t consumer_thread; \
        \
        ck_assert(pthread_create(&consumer_thread, NULL, consumer_thread_main, \
                    consumer_callback) == 0); \
        \
        producer_thread_main(producer_callback); \
        \
        ck_assert(pthread_join(consumer_thread, NULL) == 0); \
        \
        if (bytes_count) { \
            ck_assert_uint_eq(roffset_next, bytes_count); \
        } else { \
            ck_assert_uint_eq(roffset_next, DATA_SIZE); \
        } \
    } \
    END_TEST
#define CONCURRENT_TEST(...) CONCURRENT_TEST2(__VA_ARGS__, 0, 0)

/* Tests for circbuf_read/circbuf_write */

CONCURRENT_TEST(test_concurrent_normal, serial_read, serial_write,
                normal_consumer_step, normal_producer_step)

CONCURRENT_TEST(test_concurrent_slow_consumer, serial_read, serial_write,
                slow_consumer_step, normal_producer_step)

CONCURRENT_TEST(test_concurrent_slow_producer, serial_read, serial_write,
                normal_consumer_step, slow_producer_step)

CONCURRENT_TEST(test_concurrent_slow_both, serial_read, serial_write,
                slow_consumer_step, slow_producer_step)

CONCURRENT_TEST(test_concurrent_variable_both, serial_read, serial_write,
                variable_consumer_step, variable_producer_step)

CONCURRENT_TEST(test_concurrent_early_consumer_close, serial_read, serial_write,
                early_close_consumer_step, normal_producer_step, (void)0,
                EARLY_CLOSE_THRESHOLD)

CONCURRENT_TEST(test_concurrent_early_producer_close, serial_read, serial_write,
                normal_consumer_step, early_close_producer_step, (void)0,
                EARLY_CLOSE_THRESHOLD)

/* Tests for circbuf_input/circbuf_write */

CONCURRENT_TEST(test_concurrent_normal_input, serial_input, serial_write,
                normal_consumer_step, normal_producer_step)

CONCURRENT_TEST(test_concurrent_slow_consumer_input, serial_input, serial_write,
                slow_consumer_step, normal_producer_step)

CONCURRENT_TEST(test_concurrent_slow_producer_input, serial_input, serial_write,
                normal_consumer_step, slow_producer_step)

CONCURRENT_TEST(test_concurrent_slow_both_input, serial_input, serial_write,
                slow_consumer_step, slow_producer_step)

CONCURRENT_TEST(test_concurrent_variable_both_input, serial_input, serial_write,
                variable_consumer_step, variable_producer_step)

CONCURRENT_TEST(test_concurrent_early_consumer_close_input, serial_input,
                serial_write, early_close_consumer_step, normal_producer_step,
                (void)0, EARLY_CLOSE_THRESHOLD)

CONCURRENT_TEST(test_concurrent_early_producer_close_input, serial_input,
                serial_write, normal_consumer_step, early_close_producer_step,
                (void)0, EARLY_CLOSE_THRESHOLD)

/* Tests for circbuf_read/circbuf_write2 */

CONCURRENT_TEST(test_concurrent_normal_write2, serial_read, serial_write2,
                normal_consumer_step, normal_producer_step)

CONCURRENT_TEST(test_concurrent_slow_consumer_write2, serial_read, serial_write2,
                slow_consumer_step, normal_producer_step)

CONCURRENT_TEST(test_concurrent_slow_producer_write2, serial_read, serial_write2,
                normal_consumer_step, slow_producer_step)

CONCURRENT_TEST(test_concurrent_slow_both_write2, serial_read, serial_write2,
                slow_consumer_step, slow_producer_step)

CONCURRENT_TEST(test_concurrent_variable_both_write2, serial_read, serial_write2,
                variable_consumer_step, variable_producer_step)

CONCURRENT_TEST(test_concurrent_early_consumer_close_write2, serial_read,
                serial_write2, early_close_consumer_step, normal_producer_step,
                (void)0, EARLY_CLOSE_THRESHOLD)

CONCURRENT_TEST(test_concurrent_early_producer_close_write2, serial_read,
                serial_write2, normal_consumer_step, early_close_producer_step,
                (void)0, EARLY_CLOSE_THRESHOLD)

/* Loop test: _i defined by libcheck to denote iteration number. */
CONCURRENT_TEST(test_concurrent_random_both, serial_read, serial_write,
                random_consumer_step, random_producer_step, seed_base = 2 * _i)

/* Loop test: _i defined by libcheck to denote iteration number. */
CONCURRENT_TEST(test_concurrent_random_both_input, serial_input, serial_write,
                random_consumer_step, random_producer_step, seed_base = 2 * _i)

/* Loop test: _i defined by libcheck to denote iteration number. */
CONCURRENT_TEST(test_concurrent_random_both_write2, serial_read, serial_write2,
                random_consumer_step, random_producer_step, seed_base = 2 * _i)

Suite* circbuf_suite()
{
    Suite *s;
    TCase *tc;

    setup_random_data();

    s = suite_create("Circular Buffer");

    tc = tcase_create("Sequential Tests");
    tcase_add_checked_fixture(tc, setup_global, teardown_global);

    tcase_add_test(tc, test_sequential);
    tcase_add_test(tc, test_sequential_fill);
    tcase_add_test(tc, test_sequential_read_around);
    tcase_add_test(tc, test_sequential_dispose_around);
    tcase_add_test(tc, test_sequential_input_around);

    tcase_add_test(tc, test_sequential_write2);
    tcase_add_test(tc, test_sequential_fill_write2);
    tcase_add_test(tc, test_sequential_read_around_write2);

    suite_add_tcase(s, tc);

    tc = tcase_create("Concurrent Tests");
    tcase_add_checked_fixture(tc, setup_global, teardown_global);

    tcase_add_test(tc, test_concurrent_normal);
    tcase_add_test(tc, test_concurrent_early_consumer_close);
    tcase_add_test(tc, test_concurrent_early_producer_close);

    tcase_add_test(tc, test_concurrent_normal_input);
    tcase_add_test(tc, test_concurrent_early_consumer_close_input);
    tcase_add_test(tc, test_concurrent_early_producer_close_input);

    tcase_add_test(tc, test_concurrent_normal_write2);
    tcase_add_test(tc, test_concurrent_early_consumer_close_write2);
    tcase_add_test(tc, test_concurrent_early_producer_close_write2);

    suite_add_tcase(s, tc);

    tc = tcase_create("Slow Concurrent Tests");
    tcase_set_timeout(tc, SLOW_CONCURRENT_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup_global, teardown_global);

    tcase_add_test(tc, test_concurrent_slow_consumer);
    tcase_add_test(tc, test_concurrent_slow_producer);
    tcase_add_test(tc, test_concurrent_slow_both);
    tcase_add_test(tc, test_concurrent_variable_both);

    tcase_add_test(tc, test_concurrent_slow_consumer_input);
    tcase_add_test(tc, test_concurrent_slow_producer_input);
    tcase_add_test(tc, test_concurrent_slow_both_input);
    tcase_add_test(tc, test_concurrent_variable_both_input);

    tcase_add_test(tc, test_concurrent_slow_consumer_write2);
    tcase_add_test(tc, test_concurrent_slow_producer_write2);
    tcase_add_test(tc, test_concurrent_slow_both_write2);
    tcase_add_test(tc, test_concurrent_variable_both_write2);

    suite_add_tcase(s, tc);

    tc = tcase_create("Fuzzy Concurrent Tests");
    tcase_set_tags(tc, "slow");
    tcase_add_checked_fixture(tc, setup_global, teardown_global);
    tcase_add_loop_test(tc, test_concurrent_random_both, 0, 100);
    tcase_add_loop_test(tc, test_concurrent_random_both_input, 0, 100);
    tcase_add_loop_test(tc, test_concurrent_random_both_write2, 0, 100);
    suite_add_tcase(s, tc);

    return s;
}

int main(int argc, char **argv)
{
    SRunner *sr;
    int num_failed;

    sr = srunner_create(circbuf_suite());

    srunner_run_all(sr, CK_ENV);

    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
