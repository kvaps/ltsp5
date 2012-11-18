# setting config vars
if [ "${MAIN_ARCH}" = "x86" ]; then
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

if [ -z "${TIMEZONE}" ]; then
	TIMEZONE="$(</etc/timezone)"
fi

chroot_dir $CHROOT
stage_uri "${STAGE_URI}"
rootpw password
makeconf_line MAKEOPTS "${MAKEOPTS}"
makeconf_line DRACUT_MODULES "nbd nfs"

if [ -n "${MIRRORS}" ]; then
	makeconf_line GENTOO_MIRRORS "${MIRRORS}"
fi

if [ -n "${INPUT_DEVICES}" ]; then
	makeconf_line INPUT_DEVICES "${INPUT_DEVICES}"
fi

if [ -n "${VIDEO_CARDS}" ]; then
	makeconf_line VIDEO_CARDS "${VIDEO_CARDS}"
fi

if [ "${CCACHE}" == "true" ]; then
	makeconf_line FEATURES "ccache"
	makeconf_line CCACHE_SIZE "4G"
fi

locale_set "${LOCALE}"
kernel_sources gentoo-sources
kernel_builder genkernel
timezone ${TIMEZONE}
extra_packages ldm ltsp-client ${PACKAGES}


# Step control extra functions
mount_bind() {
	local source="${1}"
	local dest="${2}"

	spawn "mkdir -p ${source}"
	spawn "mkdir -p ${dest}"
	spawn "mount ${source} ${dest} -o bind"
}

post_unpack_stage_tarball() {
	# bind mounting portage and binary package dir
	mount_bind "/usr/portage" "${chroot_dir}/usr/portage"
	mount_bind "/usr/portage/packages/${ARCH}" "${chroot_dir}/usr/portage/packages"
	
	# bind mounting layman, for overlay packages
	mount_bind "/var/lib/layman" "${chroot_dir}/var/lib/layman"

	cat >> ${chroot_dir}/etc/make.conf <<- EOF
	source /var/lib/layman/make.conf
	EOF

	cat > ${chroot_dir}/etc/fstab <<- EOF
	# DO NOT DELETE
	EOF
	
	# making sure ltsp-client 5.2 is not installed
	cat > ${chroot_dir}/etc/portage/package.mask <<- EOF
	<net-misc/ltsp-client-5.3
	EOF
	
	# linking ltsp profile from overlay
	rm ${chroot_dir}/etc/make.profile
	ln -s "/var/lib/layman/ltsp/profiles/default/linux/${MAIN_ARCH}/10.0/ltsp/" "${chroot_dir}/etc/make.profile"
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
	# emerge python-2.7 to deal with "python_get_implementational_package is not installed" issues
	# these occur when emerging binary packages which are compiled against a new Python version
	spawn_chroot "emerge python:2.7"
}

post_install_extra_packages() {
	# apply localepurge
	spawn_chroot "emerge localepurge"
	cat ${chroot_dir}/etc/locale.gen | awk '{print $1}' > ${chroot_dir}/etc/locale.nopurge
	spawn_chroot "localepurge"
	spawn_chroot "emerge --unmerge localepurge"

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
