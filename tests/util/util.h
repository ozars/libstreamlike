#ifndef STREAMLIKE_TEST_UTIL_UTIL_H
#define STREAMLIKE_TEST_UTIL_UTIL_H

#include <stdlib.h>

inline void fill_random_data(char *buf, size_t len, unsigned int seed)
{
    for (int i = 0; i < len; i++) {
        buf[i] = (char)('a' + (rand_r(&seed) % 26));
    }
}

#endif /* STREAMLIKE_TEST_UTIL_UTIL_H */
