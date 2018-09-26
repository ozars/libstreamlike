TESTS = check_streamlike_file check_circbuf

AM_CPPFLAGS = -I$(top_srcdir)/src

LDADD = ../src/libstreamlike.la @CHECK_LIBS@
AM_CFLAGS = -std=gnu11 @CHECK_CFLAGS@

check_PROGRAMS = $(TESTS)
check_streamlike_file_SOURCES = check_streamlike_file.c
check_circbuf_SOURCES = check_circbuf.c
