#include <stdlib.h>
#include <string.h>
#include <check.h>

#include "circbuf.h"

#define BUFFER_SIZE (1024*1024)
#define DATA_SIZE (50*BUFFER_SIZE)

char data[DATA_SIZE];
char buf[BUFFER_SIZE];
circbuf_t *cbuf;
int roffset;

size_t compared_read(size_t len)
{
    circbuf_read(cbuf, buf, len);
    ck_assert(!memcmp(buf, data + roffset, len));
    return len;
}

size_t compared_read_some(size_t len)
{
    size_t consumed;
    consumed = circbuf_read_some(cbuf, buf, len);
    ck_assert(!memcmp(buf, data + roffset, consumed));
    return consumed;
}

size_t compared_input_some(size_t len)
{
    const void* buf;
    size_t input;
    input = circbuf_input_some(cbuf, &buf, len);
    ck_assert(!memcmp(buf, data + roffset, input));
    return input;
}

void fill_random_data()
{
    int i;
    srand(0);
    for(i = 0; i < DATA_SIZE; i++)
    {
        data[i] = (char) rand();
    }
}

START_TEST(test_sequential)
{
    cbuf = circbuf_init(BUFFER_SIZE);
    roffset = 0;

    ck_assert(circbuf_write(cbuf, data, 50) == 50);
    ck_assert(compared_read(50) == 50);
}
END_TEST

START_TEST(test_concurrent_fuzz)
{
}
END_TEST

Suite* circbuf_suite()
{
    Suite *s;
    TCase *tc;

    s = suite_create("Circular Buffer");

    tc = tcase_create("Core");
    tcase_add_test(tc, test_sequential);
    tcase_add_test(tc, test_concurrent_fuzz);

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
