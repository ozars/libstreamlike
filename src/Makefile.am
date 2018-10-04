if ENABLE_HTTP
HTTP_C = streamlike_http.c
HTTP_H = streamlike_http.h
HTTP_LIBADD = -lcurl
else
HTTP_C =
HTTP_H =
HTTP_LIBADD =
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
libstreamlike_la_CFLAGS = -std=gnu11
libstreamlike_la_LIBADD = $(HTTP_LIBADD)
include_HEADERS = streamlike.h streamlike_file.h $(HTTP_H) $(DEBUG_H)
