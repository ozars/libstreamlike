#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libstreamlike], [1.0.0-dev], [omer AT utdallas DOT edu])
AC_LANG([C])
AC_CONFIG_SRCDIR([src/streamlike.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AC_REQUIRE_AUX_FILE([tap-driver.sh])

AM_INIT_AUTOMAKE([foreign -Wall -Werror subdir-objects])

# Checks for programs.
AM_PROG_AR
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_AWK

AC_LANG_PUSH([C++])
AX_CXX_COMPILE_STDCXX([11])
AC_LANG_POP([C++])

# pkg-config
PKG_PROG_PKG_CONFIG
PKG_INSTALLDIR

# Initialize libtool to build .la files
LT_INIT

# Checks for libraries and arguments
AC_ARG_ENABLE([debug], AC_HELP_STRING([--enable-debug], [enable debug flag]))
if test "x$enable_debug" != "xyes"; then enable_debug="no"; fi
AM_CONDITIONAL([ENABLE_DEBUG], [test x$enable_debug = xyes])

AC_ARG_ENABLE([tests], AC_HELP_STRING([--enable-tests], [compile tests]))
if test "x$enable_tests" != "xyes"; then enable_tests="no"; fi
AM_CONDITIONAL([ENABLE_TESTS], [test x$enable_tests = xyes])

AC_ARG_ENABLE([http], AC_HELP_STRING([--disable-http],
              [disable streamlike_http functionality]))
if test "x$enable_http" != "xno"; then enable_http="yes"; fi
AM_CONDITIONAL([ENABLE_HTTP], [test x$enable_http = xyes])

AC_ARG_ENABLE([cpp_interface], AC_HELP_STRING([--disable-cpp-interface],
              [disable c++ interface]))
if test "x$enable_cpp_interface" != "xno"; then enable_cpp_interface="yes"; fi
AM_CONDITIONAL([ENABLE_CPP_INTERFACE], [test x$enable_cpp_interface = xyes])

AM_COND_IF([ENABLE_DEBUG], [
    AC_SUBST([STREAMLIKE_CPPFLAGS], ["-DSL_DEBUG"])
], [
    AC_SUBST([STREAMLIKE_CPPFLAGS], [""])
])

AM_COND_IF([ENABLE_TESTS], [
    PKG_CHECK_MODULES([CHECK], [check >= 0.12.0])
    AM_COND_IF([ENABLE_HTTP], [
        PKG_CHECK_MODULES([MICROHTTPD], [libmicrohttpd >= 0.9.44])
    ])
])

AM_COND_IF([ENABLE_HTTP], [
    PKG_CHECK_MODULES([CURL], [libcurl >= 7.47.0])
])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_SYS_LARGEFILE

AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 src/Makefile
                 tests/Makefile
                 streamlike.pc
])
AC_OUTPUT

AC_MSG_NOTICE([

Streamlike HTTP...$enable_http
C++ Interface.....$enable_cpp_interface
Tests.............$enable_tests
Debug Mode........$enable_debug
])
