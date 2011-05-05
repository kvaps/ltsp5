# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([nbd-proxy], [0.1], [stgraber@ltsp.org])
AC_CONFIG_SRCDIR([src/nbd-proxy.c])
AM_INIT_AUTOMAKE

AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

AC_SUBST(DEPS_CFLAGS)

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([stdio.h stdlib.h linux/nbd.h sys/socket.h pthread.h arpa/inet.h netinet/tcp.h netinet/in.h string.h sys/types.h limits.h unistd.h])

AC_CHECK_LIB(m, pow)

AC_CONFIG_FILES([Makefile src/Makefile man/Makefile])
AC_OUTPUT
