if ENABLE_HTTP
HTTP_C      = streamlike_http.c
HTTP_H      = streamlike_http.h
HTTP_CFLAGS = @CURL_CFLAGS@
HTTP_LIBS   = @CURL_LIBS@
else
HTTP_C      =
HTTP_H      =
HTTP_CFLAGS =
HTTP_LIBS   =
endif

if ENABLE_DEBUG
DEBUG_H = streamlike_debug.h
else
DEBUG_H =
endif

lib_LTLIBRARIES = libstreamlike.la
libstreamlike_la_SOURCES = streamlike.h \
						   streamlike_file.c streamlike_file.h \
						   circbuf.h circbuf.c \
						   $(HTTP_C) $(HTTP_H) \
						   $(DEBUG_H)
libstreamlike_la_CFLAGS = -std=gnu11 $(HTTP_CFLAGS)
libstreamlike_la_LIBADD = $(HTTP_LIBS)
include_HEADERS = streamlike.h streamlike_file.h $(HTTP_H) $(DEBUG_H)
