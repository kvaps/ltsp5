AC_PREREQ(2.61)
AC_INIT([ldm2],[0.1],[sbalneav@ltsp.org])
AC_CONFIG_SRCDIR([src/ldm.c])
AM_INIT_AUTOMAKE

AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([syslog.h sys/wait.h fcntl.h],,AC_MSG_ERROR(Required header missing))

dnl Checks for Libraries
AC_CHECK_LIB([util],[main],[],AC_MSG_ERROR(ldm2 requires libutil))

dnl Output
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
