install_mode chroot

if [ -z "${ARCH}" ]; then
	ARCH="x86"
fi

if [ "${ARCH}" = "x86" ]; then
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

chroot_dir $CHROOT


# TODO: much of this shell code can go into a pre install or post install script
#	   seperate from the profile

pre_sanity_check_config () {
	# we can't use the wrapper stage_uri due the way the sanity check is done in quickstart
	if [ -z "${STAGE_URI}" ]; then
		stage_uri="http://www.funtoo.org/linux/gentoo/${arch}/stage3-${arch}-current.tar.bz2"
	else 
		stage_uri="${STAGE_URI}"
	fi
}


# Skip all this
skip partition
skip setup_md_raid
skip setup_lvm
skip format_devices
skip mount_local_partitions
skip mount_network_shares
skip install_bootloader
skip configure_bootloader

tree_type none
cron none
rootpw password
tree_type none

# TODO: only skip it if we are passed a built kernel
# skip build_kernel
# TODO: do these
#kernel_config_uri
#kernel_sources (defaults to gentoo sources)
genkernel_opts --makeopts="${MAKEOPTS}"

post_unpack_stage_tarball() {
		# protecting locale.gen from updating, /etc is set in CONFIG_PROTECT_MASK
		export CONFIG_PROTECT="/etc/locale.gen"

		if [ -n "$LOCALE" ]; then
				echo "LANG=${LOCALE}" >> ${chroot_dir}/etc/env.d/02locale
				grep ${LOCALE} /usr/share/i18n/SUPPORTED > ${chroot_dir}/etc/locale.gen
		else
				if [ -f /etc/env.d/02locale ]; then
					cp /etc/env.d/02locale ${chroot_dir}/etc/env.d/
				fi

				cat > ${chroot_dir}/etc/locale.gen <<- EOF
				en_US ISO-8859-1
				en_US.UTF-8 UTF-8
				EOF
		fi
}

pre_install_portage_tree() {
	# bind mounting portage
	spawn "mkdir ${chroot_dir}/usr/portage"
	spawn "mount /usr/portage ${chroot_dir}/usr/portage -o bind"
	echo "${chroot_dir}/usr/portage" >> /tmp/install.umount

	# bind mounting binary package dir
	spawn "mkdir -p /usr/portage/packages/$ARCH"
	spawn_chroot "mkdir -p /usr/portage/packages"
	spawn "mount /usr/portage/packages/$ARCH ${chroot_dir}/usr/portage/packages -o bind"
	echo "${chroot_dir}/usr/portage/packages" >> /tmp/install.umount

	# bind mounting portage local, for overlay packages
	# TODO: remove this mounting when the ltsp ebuilds are in the tree
	spawn "mkdir ${chroot_dir}/usr/local/portage"
	spawn "mount /usr/local/portage ${chroot_dir}/usr/local/portage -o bind"
	echo "${chroot_dir}/usr/local/portage" >> /tmp/install.umount

	if [ -n "${MIRRORS}" ]; then
		echo "GENTOO_MIRRORS="${MIRRORS}"" >> ${chroot_dir}/etc/make.conf
	fi

	# TODO: allow overriding of all these variables
	cat >> ${chroot_dir}/etc/make.conf <<- EOF
	MAKEOPTS="${MAKEOPTS}"

	# TODO: test if hal and -crypt can be removed when xorg-server 1.8 is stable
	USE="alsa pulseaudio svg xml X -cups hal -crypt"

	EMERGE_DEFAULT_OPTS="--usepkg --buildpkg"
	CONFIG_PROTECT_MASK="/etc /etc/conf.d /etc/init.d"

	# TODO: don't add this by default
	source /usr/local/portage/layman/make.conf
	EOF

	cat > ${chroot_dir}/etc/fstab <<- EOF
	# DO NOT DELETE
	EOF

	# TODO: copy this from elsewhere instead of making it here.
	spawn "mkdir -p ${chroot_dir}/etc/portage/package.keywords"

	cat >> ${chroot_dir}/etc/portage/package.keywords/ltsp <<- EOF
	net-misc/ltsp-client
	sys-fs/ltspfs
	x11-misc/ldm
	EOF

	# temporary overrides (hopefully)
	cat >> ${chroot_dir}/etc/portage/package.keywords/temp <<- EOF
	sys-apps/openrc
	sys-apps/baselayout
	sys-apps/sysvinit
	EOF

	# pulseaudio pulls udev[extras]
	cat >> ${chroot_dir}/etc/portage/package.use <<- EOF
	sys-fs/udev extras
	EOF
}

pre_set_timezone() {
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
	# FIXME: upgrade to porage 2.2 to resolve blockers since 2008.0
	spawn_chroot "emerge portage"

	if [[ $CCACHE == "true" ]]; then

		#spawn_chroot "mkdir -p $TMP"

		spawn_chroot "emerge ccache"
		spawn_chroot "mkdir -p /var/tmp/ccache"
		spawn "mkdir -p /var/tmp/ccache/${ARCH}"
		spawn "mount /var/tmp/ccache/${ARCH} ${chroot_dir}/var/tmp/ccache -o bind"
		echo "${chroot_dir}/var/tmp/ccache" >> /tmp/install.umount

		cat >> ${chroot_dir}/etc/make.conf <<- EOF
		FEATURES="ccache"
		EOF

		export CCACHE_DIR="/var/tmp/ccache"
		export CCACHE_SIZE="4G"

		genkernel_opts --makeopts="${MAKEOPTS}" --kernel-cc="/usr/lib/ccache/bin/gcc" --utils-cc="/usr/lib/ccache/bin/gcc"
	fi
}

pre_install_extra_packages() {
	spawn_chroot "emerge --newuse udev"
	spawn_chroot "emerge --update --deep world"
}

extra_packages ldm ltsp-client ${PACKAGES}

post_install_extra_packages() {
	# point /etc/mtab to /proc/mounts
	spawn "ln -sf /proc/mounts ${chroot_dir}/etc/mtab"

	# make sure this is really existing before bind mounting it
	mkdir ${chroot_dir}/var/lib/nfs
 
	# TODO: test if can be removed when xorg-server 1.8 is stable
	spawn_chroot "dbus-uuidgen --ensure"
}

rcadd ltsp-client-setup boot
rcadd ltsp-client default

# TODO: test if can be removed when xorg-server 1.8 is stable
rcadd hald default
rcadd dbus default