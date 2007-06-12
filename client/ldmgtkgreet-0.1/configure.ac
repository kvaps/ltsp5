dnl Configure script for gtk greeter
AC_INIT(ldmgtkgreet)
AC_CONFIG_SRCDIR([src/greeter.c])

PACKAGE=ldmgtkgreet
VERSION=0.1

AM_INIT_AUTOMAKE($PACKAGE,$VERSION)
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE
# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
# Thanks, Ollie!
PKG_CHECK_MODULES(DEPS, gtk+-2.0 >= 2.2)
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

# Checks for header files.
AC_HEADER_STDC

AC_OUTPUT(Makefile src/Makefile pixmaps/Makefile gtkrc/Makefile)
