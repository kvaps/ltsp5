## Process this file with automake to produce Makefile.in

bin_PROGRAMS = ldmgtkgreet
ldmgtkgreet_SOURCES = greeter.c

ldmgtkgreet_LDADD = $(DEPS_LIBS)
AM_CPPFLAGS = $(DEPS_CFLAGS) \
              -DPIXMAPS_DIR=\""$(datadir)/ldm/themes/default"\" \
              -DGTKRC_DIR=\""$(datadir)/ldm/themes/default"\"
