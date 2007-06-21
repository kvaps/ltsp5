dnl Configure script for gtk greeter
AC_INIT([ldmgtkgreet], [0.1], [ogra@ubuntu.com])
AC_CONFIG_SRCDIR([src/greeter.c])
AM_INIT_AUTOMAKE

AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
# Thanks, Ollie!
dnl Checks for header files.
AC_HEADER_STDC

dnl Checks for Libraries
PKG_CHECK_MODULES(DEPS, gtk+-2.0 >= 2.2)
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

dnl Output
AC_CONFIG_FILES([Makefile src/Makefile pixmaps/Makefile gtkrc/Makefile])
AC_OUTPUT
