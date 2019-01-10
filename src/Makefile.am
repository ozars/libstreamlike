if ENABLE_HTTP
HTTP_C      = streamlike/http.c
HTTP_H      = streamlike/http.h
HTTP_CFLAGS = @CURL_CFLAGS@
HTTP_LIBS   = @CURL_LIBS@
else
HTTP_C      =
HTTP_H      =
HTTP_CFLAGS =
HTTP_LIBS   =
endif

if ENABLE_DEBUG
DEBUG_H = streamlike/debug.h
else
DEBUG_H =
endif

AM_CPPFLAGS = @STREAMLIKE_CPPFLAGS@

lib_LTLIBRARIES = libstreamlike.la
libstreamlike_la_SOURCES = streamlike.h \
                           streamlike/file.c streamlike/file.h \
                           streamlike/buffer.c streamlike/buffer.h \
                           streamlike/util/circbuf.h streamlike/util/circbuf.c \
                           $(HTTP_C) $(HTTP_H) $(DEBUG_H)
libstreamlike_la_CFLAGS = -std=gnu11 $(HTTP_CFLAGS)
libstreamlike_la_LIBADD = $(HTTP_LIBS)
nobase_include_HEADERS  = streamlike.h \
                          streamlike/file.h \
                          streamlike/buffer.h \
                          $(HTTP_H) $(DEBUG_H)
