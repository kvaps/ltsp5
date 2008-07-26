AC_PREREQ(2.59)
AC_INIT(xrexecd, [0.1], [sbalneav@ltsp.org])
AC_CONFIG_SRCDIR([src/xrexecd.c])
AM_INIT_AUTOMAKE

AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE
# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
PKG_CHECK_MODULES(XREXECD, [x11])
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.6.0)

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT

AC_OUTPUT(Makefile src/Makefile)
