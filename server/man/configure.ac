AC_PREREQ(2.59)
AC_INIT([ltsp-manpages], m4_esyscmd([grep 'VERSION' ../../release.conf | cut -d '=' -f 2 | tr -d '\n']))
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_FILES([Makefile])
AC_CHECK_PROGS([HELP2MAN], [help2man])
AC_CHECK_PROGS([LSB_RELEASE], [lsb_release])
AC_OUTPUT
