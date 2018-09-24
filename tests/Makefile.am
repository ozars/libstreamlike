TESTS = check_streamlike_file

AM_CPPFLAGS = -I$(top_srcdir)/src

check_PROGRAMS = check_streamlike_file
check_streamlike_file_SOURCES = check_streamlike_file.c
check_streamlike_file_CFLAGS = -std=gnu11 @CHECK_CFLAGS@
check_streamlike_file_LDADD = ../src/libstreamlike.la @CHECK_LIBS@
