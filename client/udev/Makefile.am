EXTRA_DIST = ltsp-sound.rules ltsp-sound

install-data-local:
	$(mkdir_p) $(DESTDIR)$(UDEV_LIB_PATH)
	$(INSTALL_DATA) -m 0755 -D $(srcdir)/ltsp-sound $(DESTDIR)$(UDEV_LIB_PATH)

uninstall-local:
	rm -f $(DESTDIR)$(UDEV_LIB_PATH)/ltsp-sound
