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
		stage_uri="http://distfiles.gentoo.org/releases/${arch}/current/stages/stage3-${arch}-2008.0.tar.bz2"
	else 
		stage_uri="${STAGE_URI}"
	fi
}
if [ -z "${TIMEZONE}" ]; then
	# For OpenRC
	if [ -e /etc/timezone ]; then
		TIMEZONE="$(</etc/timezone)"
	else
		. /etc/conf.d/clock
	fi
fi
timezone ${TIMEZONE}

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
genkernel_opts --makeopts="${MAKEOPTS}" --kernel-cc="/usr/lib/ccache/bin/gcc" --utils-cc="/usr/lib/ccache/bin/gcc"

post_unpack_stage_tarball() {
	if [ -n "$LOCALE" ]; then
		if [ -f /etc/env.d/02locale ]; then
			cp /etc/env.d/02locale ${chroot_dir}/etc/env.d/
		else
			echo "LANG=${LOCALE}" >> ${chroot_dir}/etc/env.d/02locale
			echo "LC_ALL=${LOCALE}" >> ${chroot_dir}/etc/env.d/02locale
		fi
	fi
}
pre_install_portage_tree() {
	spawn "mkdir ${chroot_dir}/usr/portage"
	spawn "mount /usr/portage ${chroot_dir}/usr/portage -o bind"
	echo "${chroot_dir}/usr/portage" >> /tmp/install.umount

	spawn "mkdir -p /usr/portage/packages/$ARCH"
	spawn_chroot "mkdir -p /usr/portage/packages"
	spawn "mount /usr/portage/packages/$ARCH ${chroot_dir}/usr/portage/packages -o bind"
	echo "${chroot_dir}/usr/portage/packages" >> /tmp/install.umount

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
	USE="alsa xml X -cups"
	VIDEO_CARDS="vesa"
	FEATURES="ccache"

	EMERGE_DEFAULT_OPTS="--usepkg --buildpkg"
	# TODO: don't add this by default
	source /usr/local/portage/layman/make.conf
	EOF

	cat > ${chroot_dir}/etc/fstab <<- EOF
	# DO NOT DELETE
	EOF
   
	# TODO: copy the preset version of /etc/portage from elsewhere
	#	   instead of making it here
	spawn "mkdir -p ${chroot_dir}/etc/portage/package.keywords"

	cat >> ${chroot_dir}/etc/portage/package.keywords/ltsp <<- EOF
	net-misc/ltsp-client
	sys-apps/openrc
	sys-apps/baselayout
	sys-fs/ltspfs
	x11-misc/ldm
	# needed for ldm
	=x11-themes/gtk-engines-ubuntulooks-0.9.12*

	EOF

	# temporary overrides (hopefully)
	cat >> ${chroot_dir}/etc/portage/package.keywords/temp <<- EOF
	# stable fails on 2.6.24 kernel
	~sys-fs/fuse-2.7.3
	EOF
}

pre_build_kernel() {
	export CONFIG_PROTECT_MASK=""
	export CCACHE_DIR="/var/tmp/ccache"
	export CCACHE_SIZE="4G"

	spawn_chroot "mkdir -p $TMP"

	spawn_chroot "emerge ccache"
	spawn_chroot "mkdir -p /var/tmp/ccache"
	spawn "mkdir -p /var/tmp/ccache/${ARCH}"
	spawn "mount /var/tmp/ccache/${ARCH} ${chroot_dir}/var/tmp/ccache -o bind"
	echo "${chroot_dir}/var/tmp/ccache" >> /tmp/install.umount
}

pre_install_extra_packages() {
	spawn_chroot "emerge --update --deep world"
}

extra_packages joystick ltspfs ldm ltsp-client xdm ${PACKAGES}

post_install_extra_packages() {
	# point /etc/mtab to /proc/mounts
	spawn "ln -sf /proc/mounts ${chroot_dir}/etc/mtab"

	# make sure this is really existing before bind mounting it
	mkdir ${chroot_dir}/var/lib/nfs

	# Set a default hostname
	echo 'HOSTNAME="ltsp"' > ${chroot_dir}/etc/conf.d/hostname
 
	spawn_chroot "rm /etc/init.d/net.eth0"

	cat >> ${chroot_dir}/etc/lts.conf <<- EOF
	# see /usr/share/doc/ltsp-client-$version/lts-parameters.txt.bz2 for a listing 
	# of all possible options (Don't forget to include a [default] section)
	# TODO: dont serve this file from here
	#	   put it in /var/lib/tftpboot/ltsp/arch/lts.conf
	EOF
}

rcadd ltsp-client-setup boot
rcadd ltsp-client default
