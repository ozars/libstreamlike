if ENABLE_TESTS

if ENABLE_HTTP
HTTP_TEST    = check_streamlike_http
HTTP_SOURCES = check_streamlike_http.c
HTTP_CFLAGS  = @CURL_CFLAGS@ @MICROHTTPD_CFLAGS@
HTTP_LIBS    = @CURL_LIBS@ @MICROHTTPD_LIBS@
else
HTTP_TEST    =
HTTP_SOURCES =
HTTP_CFLAGS  =
HTTP_LIBS    =
endif

LOG_DRIVER = env CK_TAP_LOG_FILE_NAME='-' AM_TAP_AWK='$(AWK)' \
			 '$(SHELL)' '$(top_srcdir)/build-aux/tap-driver.sh'

TESTS = check_streamlike_file check_circbuf $(HTTP_TEST)

AM_CPPFLAGS = -I$(top_srcdir)/src @STREAMLIKE_CPPFLAGS@

LDADD = ../src/libstreamlike.la @CHECK_LIBS@
AM_CFLAGS = -std=gnu11 @CHECK_CFLAGS@

check_PROGRAMS = $(TESTS)

check_streamlike_file_SOURCES = check_streamlike_file.c

check_streamlike_http_SOURCES = $(HTTP_SOURCES)
check_streamlike_http_CFLAGS  = $(CFLAGS) $(HTTP_CFLAGS)
check_streamlike_http_LDADD   = $(LDADD) $(HTTP_LIBS)

check_circbuf_SOURCES = check_circbuf.c

endif
