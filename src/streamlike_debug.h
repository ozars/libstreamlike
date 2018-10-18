#ifndef SL_LOG
# ifdef STREAMLIKE_DEBUG
#  include <stdio.h>
#  define SL_LOG_COLOR_START "\e[96m\e[40m"
#  define SL_LOG_COLOR_END   "\e[0m"
#  define SL_LOG_PREFIX      SL_LOG_COLOR_START "%%LOG: "
#  define SL_LOG_POSTFIX     "\n" SL_LOG_COLOR_END
#  define SL_LOG(msg, ...) \
    do { \
        fprintf(stderr, SL_LOG_PREFIX "%s:%d:%s: " msg SL_LOG_POSTFIX, \
                __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    } while(0)
# else
#  define SL_LOG(...) ((void)0)
# endif
#endif

#ifndef SL_ASSERT
# if defined(SL_DEBUG)
#  include <assert.h>
/**
 * Asertion is enabled since `SL_DEBUG` was defined. Standard C library
 * assertion is used by default. `SL_ASSERT` can be defined by user before
 * including this header file.
 */
#  define SL_ASSERT(x) assert(x)
# else
/**
 * Asertion is disabled since `SL_DEBUG` was not defined. Standard C library
 * assertion is used when enabled. `SL_ASSERT` can be defined by user before
 * including this header file.
 */
#  define SL_ASSERT(x) ((void)0)
# endif
#endif
