if ENABLE_STREAMLIKE_HTTP
STREAMLIKE_HTTP_C = streamlike_http.c
STREAMLIKE_HTTP_H = streamlike_http.h
else
STREAMLIKE_HTTP_C =
STREAMLIKE_HTTP_H =
endif

lib_LTLIBRARIES = libstreamlike.la
libstreamlike_la_SOURCES = streamlike.h \
						   streamlike_file.c streamlike_file.h \
						   $(STREAMLIKE_HTTP_C) $(STREAMLIKE_HTTP_H)
libstreamlike_la_CFLAGS = -std=gnu11
include_HEADERS = streamlike.h streamlike_file.h $(STREAMLIKE_HTTP_H)
