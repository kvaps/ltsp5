# setting config vars
[ "${MAIN_ARCH}" = "x86" ] && use_linux32
[ -z "${BASE}" ] && BASE="/opt/ltsp"
[ -z "${NAME}" ] && NAME="${ARCH}"
[ -z "${LOCALE}" ] && LOCALE="en_US.UTF-8"
[ -z "${TIMEZONE}" ] && TIMEZONE="$(</etc/timezone)"

chroot_dir "${BASE}/${NAME}"
stage_uri "${STAGE_URI}"
rootpw password
makeconf_line MAKEOPTS "${MAKEOPTS}"
locale_set "${LOCALE}"
kernel_sources "${KERNEL_SOURCES}"
kernel_builder genkernel
genkernel_kernel_opts --makeopts="${MAKEOPTS}"
genkernel_initramfs_opts --makeopts="${MAKEOPTS}"
initramfs_builder "${INITRAMFS_BUILDER}"
timezone ${TIMEZONE}
extra_packages ldm ltsp-client ${PACKAGES}

[ -n "${MIRRORS}" ] && makeconf_line GENTOO_MIRRORS "${MIRRORS}"
[ -n "${INPUT_DEVICES}" ] && makeconf_line INPUT_DEVICES "${INPUT_DEVICES}"
[ -n "${VIDEO_CARDS}" ] && makeconf_line VIDEO_CARDS "${VIDEO_CARDS}"
[ "${CCACHE}" = "true" ] && makeconf_line FEATURES "ccache" && makeconf_line CCACHE_SIZE "4G"
[ -n "${KERNEL_CONFIG_URI}" ] && kernel_config_uri "${KERNEL_CONFIG_URI}"


# Step control extra functions
mount_bind() {
    local source="${1}"
    local dest="${2}"

    spawn "mkdir -p ${source}"
    spawn "mkdir -p ${dest}"
    spawn "mount ${source} ${dest} -o bind"
}

post_unpack_stage_tarball() {
    # setting server portage vars
    local server_pkgdir=$(portageq pkgdir)
    local server_distdir=$(portageq distdir)

    # bind mounting portage, layman and binary package dirs
    mount_bind "/usr/portage" "${chroot_dir}/usr/portage"
    mount_bind "${server_pkgdir}/${ARCH}" "${chroot_dir}/usr/portage/packages"
    mount_bind "/var/lib/layman" "${chroot_dir}/var/lib/layman"

    # mount distfiles if at non default location
    if [ "${server_distdir}" != "/usr/portage/distfiles" ]; then
        mount_bind ${server_distdir} "${chroot_dir}/usr/portage/distfiles"
    fi

    echo "source /var/lib/layman/make.conf" >> ${chroot_dir}/etc/portage/make.conf
    echo "# DO NOT DELETE" >> ${chroot_dir}/etc/fstab

    # so ltsp-chroot knows which arch to package mount
    mkdir ${chroot_dir}/etc/ltsp
    echo "${ARCH}" > ${chroot_dir}/etc/ltsp/arch.conf

    # linking ltsp profile from overlay
    rm ${chroot_dir}/etc/portage/make.profile
    ln -s "/var/lib/layman/ltsp/profiles/default/linux/${MAIN_ARCH}/13.0/ltsp/" "${chroot_dir}/etc/portage/make.profile"
}

post_install_initramfs_builder() {
    if [ "${initramfs_builder}" = "genkernel" ]; then
        # add you're own network drivers if needed
        # eg. "MODULES_NET=\"\${MODULES_NET}\" via-rhine"
        echo "MODULES_NET=\"\${MODULES_NET}\"" >> "${chroot_dir}/usr/share/genkernel/arch/${MAIN_ARCH}/modules_load"
    fi
}

pre_build_kernel() {
    if [ "${CCACHE}" = "true" ]; then
        spawn_chroot "emerge ccache"
        mount_bind "/var/tmp/ccache/${ARCH}" "${chroot_dir}/var/tmp/ccache"
        genkernel_kernel_opts --makeopts="${MAKEOPTS}" --kernel-cc="/usr/lib/ccache/bin/gcc" --utils-cc="/usr/lib/ccache/bin/gcc"
        genkernel_initramfs_opts --makeopts="${MAKEOPTS}" --kernel-cc="/usr/lib/ccache/bin/gcc" --utils-cc="/usr/lib/ccache/bin/gcc"
    fi
}

pre_build_initramfs() {
    if [ "${initramfs_builder}" = "dracut" ]; then
        moduledir=$(ls -1r ${chroot_dir}/lib/modules | head -n 1)
        kernelversion=$(echo ${moduledir} | cut -d "-" -f 1)
        name="initramfs-dracut-${MAIN_ARCH}-${kernelversion}-gentoo"
        dracut_initramfs_opts -m \"kernel-modules nbd nfs network base\" --filesystems \"squashfs\" /boot/${name} ${moduledir}
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

    # remove possible dependencies of excluded
    spawn_chroot "emerge --depclean"

    # point /etc/mtab to /proc/mounts
    spawn "ln -sf /proc/mounts ${chroot_dir}/etc/mtab"

    # make sure these exist
    mkdir -p ${chroot_dir}/var/lib/nfs
    mkdir -p ${chroot_dir}/var/lib/pulse

    # required for openrc's bootmisc
    mkdir -p ${chroot_dir}/var/lib/misc
}
