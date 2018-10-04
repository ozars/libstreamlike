#include <stdlib.h>
#include <string.h>
#include <check.h>
#include <pthread.h>
#include <unistd.h>

#include "streamlike_http.c"

streamlike_t *stream;
pid_t server_pid;

/* void* run_test_server(void *arg) */
/* { */
/*     if ((server_pid = fork()) == 0) { */
/*     } */
/* } */

void setup_global()
{
    stream = sl_http_create("http://www.example.com/");
    ck_assert(stream);
}

void teardown_global()
{
    ck_assert(sl_http_destroy(stream) == 0);
}

START_TEST(test_simple_read)
{
    char buffer[8192];
    size_t len;
    len = sl_read(stream, buffer, sizeof(buffer));
    SL_LOG("HTTP data: \n%.*s", (int)len, buffer);
}
END_TEST

START_TEST(test_partitioned_read)
{
    char buffer[512];
    size_t len;
    do {
        len = sl_read(stream, buffer, sizeof(buffer));
        SL_LOG("HTTP data partition (len: %zu): \n%.*s", len, (int)len, buffer);
    } while(len == sizeof(buffer));
}
END_TEST

Suite* streamlike_http_suite()
{
    Suite *s;
    TCase *tc;

    s = suite_create("Streamlike HTTP");

    tc = tcase_create("Sequential Tests");
    tcase_add_checked_fixture(tc, setup_global, teardown_global);
    tcase_add_test(tc, test_simple_read);
    tcase_add_test(tc, test_partitioned_read);
    suite_add_tcase(s, tc);

    return s;
}

int main(int argc, char **argv)
{
    SRunner *sr;
    int num_failed;

    sr = srunner_create(streamlike_http_suite());

    srunner_run_all(sr, CK_ENV);

    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
