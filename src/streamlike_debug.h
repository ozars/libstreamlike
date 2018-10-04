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
