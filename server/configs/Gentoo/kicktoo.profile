# setting config vars
if [ "${ARCH}" = "i486" ] || [ "${ARCH}" = "i686" ]; then
	use_linux32
fi

if [ -z "${BASE}" ]; then
	BASE="/opt/ltsp"
fi

if [ -z "${NAME}" ]; then
	NAME="${ARCH}"
fi

if [ -z "${CHROOT}" ]; then
	CHROOT="${BASE}/${NAME}"
fi

if [ -z "${LOCALE}" ]; then
	LOCALE="en_US.UTF-8"
fi

chroot_dir $CHROOT
stage_uri "${STAGE_URI}"
rootpw password
makeconf_line MAKEOPTS "${MAKEOPTS}"
makeconf_line USE "alsa pulseaudio svg xml X -cups"
makeconf_line EMERGE_DEFAULT_OPTS "--usepkg --buildpkg"
makeconf_line CONFIG_PROTECT_MASK "/etc /etc/conf.d /etc/init.d"
makeconf_line CLEAN_DELAY 0
makeconf_line EMERGE_WARNING_DELAY 0
if [ -n "${MIRRORS}" ]; then
	makeconf_line GENTOO_MIRRORS "${MIRRORS}"
fi
if [ "${CCACHE}" == "true" ]; then
	makeconf_line FEATURES "ccache"
	makeconf_line CCACHE_SIZE "4G"
fi
locale_set "${LOCALE}"
kernel_sources gentoo-sources
kernel_builder genkernel
timezone UTC
extra_packages ldm ltsp-client dejavu sysklogd ${PACKAGES}
rcadd sysklogd default
rcadd ltsp-client-setup boot
rcadd ltsp-client default


# Step control extra functions
mount_bind() {
	local source="${1}"
	local dest="${2}"

	spawn "mkdir -p ${source}"
	spawn "mkdir -p ${dest}"
	spawn "mount ${source} ${dest} -o bind"
	echo "${dest}" >> /tmp/install.umount
}

post_unpack_stage_tarball() {
	# bind mounting portage and binary package dir
	mount_bind "/usr/portage" "${chroot_dir}/usr/portage"
	mount_bind "/usr/portage/packages/${ARCH}" "${chroot_dir}/usr/portage/packages"
	
	# bind mounting layman, for overlay packages
	# TODO: remove this mounting when the ltsp ebuilds are in the tree
	mount_bind "/var/lib/layman" "${chroot_dir}/var/lib/layman"

	# TODO: don't add this by default
	cat >> ${chroot_dir}/etc/make.conf <<- EOF
	source /var/lib/layman/make.conf
	EOF

	cat > ${chroot_dir}/etc/fstab <<- EOF
	# DO NOT DELETE
	EOF

	# TODO: copy this from elsewhere instead of making it here.
	# remove packages from here when they are stable
	spawn "mkdir -p ${chroot_dir}/etc/portage"

	cat >> ${chroot_dir}/etc/portage/package.keywords <<- EOF
	net-misc/ltsp-client
	x11-misc/ldm
	EOF

	cat >> ${chroot_dir}/etc/portage/package.use <<- EOF
	# req by pulseaudio
	sys-fs/udev extras
	# req by xorg-server
	dev-libs/libxml2 python
	# req by mesa
	x11-libs/libdrm libkms
	EOF
}

pre_setup_timezone() {
	# retrieve chroot timezone from server
	if [ -z "${TIMEZONE}" ]; then
		# For OpenRC
		if [ -e /etc/timezone ]; then
			TIMEZONE="$(</etc/timezone)"
		else
			. /etc/conf.d/clock
		fi
	fi
	timezone ${TIMEZONE}
}

pre_build_kernel() {
	if [ -n "${KERNEL_CONFIG_URI}" ]; then
		kernel_config_uri "${KERNEL_CONFIG_URI}"
	fi

	if [ -n "${KERNEL_SOURCES}" ]; then
 		kernel_sources "${KERNEL_SOURCES}"
 	fi

    genkernel_opts --makeopts="${MAKEOPTS}"

	if [ "${CCACHE}" == "true" ]; then
		spawn_chroot "emerge ccache"
		mount_bind "/var/tmp/ccache/${ARCH}" "${chroot_dir}/var/tmp/ccache"
		genkernel_opts --makeopts="${MAKEOPTS}" --kernel-cc="/usr/lib/ccache/bin/gcc" --utils-cc="/usr/lib/ccache/bin/gcc"
	fi
}

pre_install_extra_packages() {
	spawn_chroot "emerge --newuse udev"
	spawn_chroot "emerge --update --deep world"
}

post_install_extra_packages() {
	# remove excluded packages
	for package in ${EXCLUDE}; do
		spawn_chroot "emerge --unmerge ${package}"
	done

	# point /etc/mtab to /proc/mounts
	spawn "ln -sf /proc/mounts ${chroot_dir}/etc/mtab"

	# make sure these exist
	mkdir -p ${chroot_dir}/var/lib/nfs
	mkdir -p ${chroot_dir}/var/lib/pulse
	
	# required for openrc's bootmisc
	mkdir -p ${chroot_dir}/var/lib/misc
}
