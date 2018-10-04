#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libstreamlike], [1.0.0], [omer AT utdallas DOT edu])
AC_LANG([C])
AC_CONFIG_SRCDIR([src/streamlike.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])

AM_INIT_AUTOMAKE([foreign -Wall -Werror])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_AR
AC_PROG_LIBTOOL

# Initialize libtool to build .la files
LT_INIT

# Checks for libraries and arguments

AC_ARG_ENABLE([debug], AC_HELP_STRING([--enable-debug], [enable debug flag]))
AM_CONDITIONAL([ENABLE_DEBUG], [test x$enable_debug = xyes])

AC_ARG_ENABLE([tests], AC_HELP_STRING([--enable-tests], [compile tests]))
AM_CONDITIONAL([ENABLE_TESTS], [test x$enable_tests = xyes])

AC_ARG_ENABLE([http], AC_HELP_STRING([--disable-http],
              [disable streamlike_http functionality]))
AM_CONDITIONAL([ENABLE_HTTP], [test x$enable_http = xyes])

AM_COND_IF([ENABLE_DEBUG], [
    AC_SUBST([STREAMLIKE_CPPFLAGS], ["-DSTREAMLIKE_DEBUG"])
], [
    AC_SUBST([STREAMLIKE_CPPFLAGS], [""])
])

AM_COND_IF([ENABLE_TESTS], [
    PKG_CHECK_MODULES([CHECK], [check >= 0.12.0])
    AM_COND_IF([ENABLE_HTTP], [
        PKG_CHECK_MODULES([LIBMICROHTTPD], [libmicrohttpd >= 0.9.44])
    ])
])

AM_COND_IF([ENABLE_HTTP], [
    PKG_CHECK_MODULES([CURL], [libcurl >= 7.47.0])
])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FSEEKO
AC_FUNC_MALLOC

AC_SYS_LARGEFILE

AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 src/Makefile
                 tests/Makefile])
AC_OUTPUT
