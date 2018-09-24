#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libstreamlike], [1.0.0], [omer AT utdallas DOT edu])
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

# Checks for libraries.

# libcheck is required for running tests through make check target. make check
# will fail if libcheck is not found.
PKG_CHECK_MODULES([CHECK], [check >= 0.12.0], have_libcheck=true,
                  have_libcheck=false)
AM_CONDITIONAL([HAVE_LIBCHECK], [test x$have_libcheck = xtrue])

# libcurl is required for streamlike_http, which is optional.
AC_ARG_WITH([streamlike-http],
    AC_HELP_STRING([--without-streamlike-http],
                   [Disable streamlike_http functionality]))
AS_IF([test x$with_streamlike_http != xno],
    PKG_CHECK_MODULES([CURL], [libcurl], have_libcurl=true, have_libcurl=false)
    AM_CONDITIONAL([ENABLE_STREAMLIKE_HTTP], [test x$have_libcurl = xtrue]),
    AM_CONDITIONAL([ENABLE_STREAMLIKE_HTTP], 0))

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
