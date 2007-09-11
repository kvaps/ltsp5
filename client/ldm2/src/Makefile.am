bin_PROGRAMS = ldm
ldm_SOURCES = ldm.c sshutils.c greeter_iface.c get_ip.c ldm.h

ldm_LDADD = $(GLIB_LIBS) $(GOBJECT_LIBS)
AM_CPPFLAGS = $(GLIB_CFLAGS) \
              $(GOBJECT_CFLAGS) \
              -DRC_DIR=\""$(datadir)/ldm"\"
