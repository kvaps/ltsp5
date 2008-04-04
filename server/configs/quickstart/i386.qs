use_linux32
chroot_dir /opt/ltsp/i386

pre_sanity_check_config () { 
    # Get variables we need (we could use a seperate pre install script)
    # As far as i can tell, we can't inject into the quickstart configuration
    # with our own environment variables any other place than here, since
    # many variables are evaluated before the script is actually ran
    if [ -z "${STAGE_URI}" ]; then
        # TODO: switch this to current when 2008.0 leaves beta
        stage_uri="http://distfiles.gentoo.org/releases/${arch}/2008.0_beta1/stage3-${arch}-2008.0_beta1.tar.bz2"
    else 
        stage_uri="${STAGE_URI}"
    fi
    
    timezone="${TZ}"
    extra_packages="joystick ltspfs ldm ltsp-client pulseaudio xorg-server ${PACKAGES}"
    
        
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
skip build_kernel
# TODO: do these
#kernel_config_uri
#kernel_sources (defaults to gentoo sources)
#genkernel_opts

post_unpack_stage_tarball() {
    if [ -f /etc/localtime ]; then
        cp /etc/localtime ${chroot_dir}/etc/localtime
    fi

    if [ -n "$LOCALE" ]; then
        if [ -f /etc/env.d/02locale ]; then
            cp /etc/env.d/02locale ${chroot_dir}/etc/env.d/
        else
            echo "LANG=$LOCALE" >> ${chroot_dir}/etc/env.d/02locale
            echo "LC_ALL=$LOCALE" >> ${chroot_dir}/etc/env.d/02locale
        fi
    fi
}
pre_install_portage_tree() {
    spawn "mkdir ${chroot_dir}/usr/portage"
    spawn "mount /usr/portage ${chroot_dir}/usr/portage -o bind"

    cat >> ${chroot_dir}/etc/make.conf <<EOF
MAKEOPTS="${MAKEOPTS}"
USE="xml"
VIDEO_CARDS="vesa"
GENTOO_MIRRORS="${MIRRORS}"
EMERGE_DEFAULT_OPTS="--usepkg"
source /usr/portage/local/layman/make.conf
EOF

    cat > ${chroot_dir}/etc/fstab <<EOF
#Dynamically replaced on client boot
EOF

    mkdir ${chroot_dir}/etc/portage
}

pre_install_extra_packages() {
    spawn_chroot "emerge --update --deep world"
    # TODO: should layman have a pkg_config in the ebuild?
}

post_install_extra_packages() {
    # point /etc/mtab to /proc/mounts
    ln -sf /proc/mounts ${chroot_dir}/etc/mtab

    # Avoid fsck
    touch ${chroot_dir}/fastboot

    # make sure this is really existing before bind mounting it
    mkdir ${chroot_dir}/var/lib/nfs

    # Set a default hostname
    echo 'HOSTNAME="ltsp"' > $ROOT/etc/conf.d/hostname
}

rcadd ltsp-client-setup boot
rcadd ltsp-client default
