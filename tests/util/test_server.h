#ifndef STREAMLIKE_TEST_UTIL_TEST_SERVER
#define STREAMLIKE_TEST_UTIL_TEST_SERVER

#include <stdint.h>
#include <stddef.h>

typedef struct test_server_s test_server_t;

test_server_t* test_server_run(const char *content, size_t content_len);
void test_server_stop(test_server_t *test_server);
uint16_t test_server_port(test_server_t *test_server);
const char* test_server_address(test_server_t *test_server);

#endif /* STREAMLIKE_TEST_UTIL_TEST_SERVER */
