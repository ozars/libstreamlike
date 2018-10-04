if ENABLE_TESTS

if ENABLE_HTTP
HTTP_TEST = check_streamlike_http
HTTP_SOURCES = check_streamlike_http.c
HTTP_LIBS = -lcurl
else
HTTP_TEST =
HTTP_LIBS =
endif

TESTS = check_streamlike_file check_circbuf $(HTTP_TEST)

AM_CPPFLAGS = -I$(top_srcdir)/src @STREAMLIKE_CPPFLAGS@

LDADD = ../src/libstreamlike.la @CHECK_LIBS@
AM_CFLAGS = -std=gnu11 @CHECK_CFLAGS@

check_PROGRAMS = $(TESTS)
check_streamlike_file_SOURCES = check_streamlike_file.c
check_streamlike_http_SOURCES = $(HTTP_SOURCES)
check_streamlike_http_LDADD = $(LDADD) $(HTTP_LIBS)
check_circbuf_SOURCES = check_circbuf.c
endif
