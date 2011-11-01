EXTRA_DIST = 88-ltsp-sound.rules ltsp-sound

UDEV_RULES_PATH = @UDEV_RULES_PATH@

install-data-local:
	$(mkdir_p) $(DESTDIR)$(UDEV_RULES_PATH)
	$(INSTALL_DATA) $(srcdir)/88-ltsp-sound.rules $(DESTDIR)$(UDEV_RULES_PATH)
	$(mkdir_p) $(DESTDIR)$(UDEV_LIB_PATH)
	$(INSTALL_DATA) -m 0755 -D $(srcdir)/ltsp-sound $(DESTDIR)$(UDEV_LIB_PATH)

uninstall-local:
	rm -f $(DESTDIR)$(UDEV_RULES_PATH)/88-ltsp-sound.rules
	rm -f $(DESTDIR)$(UDEV_LIB_PATH)/ltsp-sound
