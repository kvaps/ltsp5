EXTRA_DIST = ltsp-sound.rules ltsp-sound

UDEV_RULES_PATH = /usr/share/ltsp/udev/

install-data-local:
	$(mkdir_p) $(DESTDIR)$(UDEV_RULES_PATH)
	$(INSTALL_DATA) $(srcdir)/ltsp-sound.rules $(DESTDIR)$(UDEV_RULES_PATH)
	$(mkdir_p) $(DESTDIR)$(UDEV_LIB_PATH)
	$(INSTALL_DATA) -m 0755 -D $(srcdir)/ltsp-sound $(DESTDIR)$(UDEV_LIB_PATH)

uninstall-local:
	rm -f $(DESTDIR)$(UDEV_RULES_PATH)/ltsp-sound.rules
	rm -f $(DESTDIR)$(UDEV_LIB_PATH)/ltsp-sound
