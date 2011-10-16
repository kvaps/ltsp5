# setting config vars
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
stage_uri "${STAGE_URI}"
rootpw password
makeconf MAKEOPTS "${MAKEOPTS}"
makeconf USE "alsa pulseaudio svg xml X -cups"
makeconf EMERGE_DEFAULT_OPTS "--usepkg --buildpkg"
makeconf CONFIG_PROTECT_MASK "/etc /etc/conf.d /etc/init.d"
makeconf CLEAN_DELAY 0
makeconf EMERGE_WARNING_DELAY 0
if [ -n "${MIRRORS}" ]; then
	makeconf GENTOO_MIRRORS "${MIRRORS}"
fi
kernel_sources gentoo-sources
kernel_builder genkernel
timezone UTC
extra_packages ldm ltsp-client dejavu sysklogd ${PACKAGES}
rcadd sysklogd default
rcadd ltsp-client-setup boot
rcadd ltsp-client default


# Step control extra functions

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

	# bind mounting portage
	spawn "mkdir ${chroot_dir}/usr/portage"
	spawn "mount /usr/portage ${chroot_dir}/usr/portage -o bind"
	echo "${chroot_dir}/usr/portage" >> /tmp/install.umount

	# bind mounting binary package dir
	spawn "mkdir -p /usr/portage/packages/$ARCH"
	spawn_chroot "mkdir -p /usr/portage/packages"
	spawn "mount /usr/portage/packages/$ARCH ${chroot_dir}/usr/portage/packages -o bind"
	echo "${chroot_dir}/usr/portage/packages" >> /tmp/install.umount

	# bind mounting layman, for overlay packages
	# TODO: remove this mounting when the ltsp ebuilds are in the tree
	spawn "mkdir -p ${chroot_dir}/var/lib/layman"
	spawn "mount /var/lib/layman ${chroot_dir}/var/lib/layman -o bind"
	echo "${chroot_dir}/var/lib/layman" >> /tmp/install.umount

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

	# pulseaudio pulls udev[extras]
	cat >> ${chroot_dir}/etc/portage/package.use <<- EOF
	sys-fs/udev extras
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

	# perhaps in kicktoo program
	if [[ $CCACHE == "true" ]]; then
		run_emerge ccache
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
