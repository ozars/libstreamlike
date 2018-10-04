if ENABLE_HTTP
HTTP_C = streamlike_http.c
HTTP_H = streamlike_http.h
HTTP_LIBADD = -lcurl
else
HTTP_C =
HTTP_H =
HTTP_LIBADD =
endif

lib_LTLIBRARIES = libstreamlike.la
libstreamlike_la_SOURCES = streamlike.h \
						   streamlike_file.c streamlike_file.h \
						   $(HTTP_C) $(HTTP_H) \
						   circbuf.h circbuf.c
libstreamlike_la_CFLAGS = -std=gnu11
libstreamlike_la_LIBADD = $(HTTP_LIBADD)
include_HEADERS = streamlike.h streamlike_file.h $(HTTP_H)
