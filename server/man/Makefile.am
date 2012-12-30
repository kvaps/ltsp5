dist_man_MANS = ltsp-chroot.8 ltsp-config.8 ltsp-info.1 \
	ltsp-update-image.8 ltsp-update-kernels.8 ltsp-update-sshkeys.8
H2M_COMMON_FLAGS = -N --version-string=$(VERSION) --include include/common.h2m

# postponing this since this requires ltsp-build-client to be actually installed
#ltsp-build-client.8: ../ltsp-build-client
#	$(HELP2MAN) -o ltsp-build-client.8 -s 8 $(H2M_COMMON_FLAGS) \
#	--name="build an LTSP client system for use with the LTSP server" \
#	--help-option="--extra-help" ../ltsp-build-client

ltsp-chroot.8: ../ltsp-chroot
	MODULES_BASE="../share/ltsp" $(HELP2MAN) -o ltsp-chroot.8 \
	-s 8 $(H2M_COMMON_FLAGS) \
	--name="chroot into a specific LTSP thin client chroot" \
	../ltsp-chroot

ltsp-config.8: ../ltsp-config
	MODULES_BASE="../share/ltsp" $(HELP2MAN) -o ltsp-config.8 \
	-s 8 $(H2M_COMMON_FLAGS) \
	 --name="configure certain parts of an LTSP server" \
	../ltsp-config

ltsp-info.1: ../ltsp-info
	MODULES_BASE="../share/ltsp" $(HELP2MAN) -o ltsp-info.1 \
	-s 1 $(H2M_COMMON_FLAGS) \
	--name="display troubleshooting information about ltsp server and chroots" \
	../ltsp-info

ltsp-update-image.8: ../ltsp-update-image
	MODULES_BASE="../share/ltsp" $(HELP2MAN) -o ltsp-update-image.8 \
	-s 8 $(H2M_COMMON_FLAGS) \
	--name="generate an NBD image from an LTSP chroot" \
	../ltsp-update-image

ltsp-update-kernels.8: ../ltsp-update-kernels
	MODULES_BASE="../share/ltsp" $(HELP2MAN) -o ltsp-update-kernels.8 \
	-s 8 $(H2M_COMMON_FLAGS) \
	--name="copy LTSP chroot kernels to TFTP directories" \
	../ltsp-update-kernels

ltsp-update-sshkeys.8: ../ltsp-update-sshkeys
	MODULES_BASE="../share/ltsp" $(HELP2MAN) -o ltsp-update-sshkeys.8 \
	-s 8 $(H2M_COMMON_FLAGS) \
	--name="update LTSP chroots to trust the server ssh keys" \
	../ltsp-update-sshkeys
