use_linux32
chroot_dir /opt/ltsp/i386

# TODO: most of this shell code can go into a pre install or post install script
#       seperate from the profile

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
    
    if [ -z "${TIMEZONE}" ]; then
        # For OpenRC
        if [ -e /etc/timezone ]; then
            TIMEZONE="$(</etc/timezone)"
        else
            . /etc/conf.d/clock
        fi
    fi

    timezone="${TIMEZONE}"
    extra_packages="joystick ltspfs ldm ltsp-client xdm ${PACKAGES}"
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
#genkernel_opts

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

    if [ -n "${MIRRORS}" ]; then
        echo "GENTOO_MIRRORS="${MIRRORS}"" >> ${chroot_dir}/etc/make.conf
    fi

    # TODO: allow overriding of all these variables
    cat >> ${chroot_dir}/etc/make.conf <<EOF
MAKEOPTS="${MAKEOPTS}"
USE="xml X -cups"
VIDEO_CARDS="vesa"

EMERGE_DEFAULT_OPTS="--usepkg"
# TODO: don't add this by default
source /usr/portage/local/layman/make.conf
EOF

    cat > ${chroot_dir}/etc/fstab <<EOF
# DO NOT DELETE
EOF
   
    # TODO: copy the contents of /etc/portage from elsewhere
    spawn "mkdir -p ${chroot_dir}/etc/portage/package.keywords"
    spawn "mkdir -p ${chroot_dir}/etc/portage/package.unmask"
    spawn "mkdir -p ${chroot_dir}/etc/portage/package.use"

    cat >> ${chroot_dir}/etc/portage/package.keywords/ltsp <<EOF
net-misc/ltsp-client
x11-misc/ldm
sys-fs/ltspfs
sys-apps/openrc
sys-apps/baselayout
x11-themes/gtk-engines-ubuntulooks
EOF

# temporary overrides (hopefully)
    cat >> ${chroot_dir}/etc/portage/package.keywords/temp <<EOF
# needed for baselayout2
=sys-fs/udev-118*
# stable fails on 2.6.24 kernel
=sys-fs/fuse-2.7.2*
# stable version didn't compile
=sys-block/nbd-2.9.2
# needed for ldm
x11-themes/gtk-engines-ubuntulooks
EOF

    cat >> ${chroot_dir}/etc/portage/package.unmask/ltsp <<EOF
EOF

    cat >> ${chroot_dir}/etc/portage/package.use/ltsp <<EOF
EOF
}

pre_install_extra_packages() {
    spawn_chroot "emerge --update --deep world"
}

post_install_extra_packages() {
    # point /etc/mtab to /proc/mounts
    spawn "ln -sf /proc/mounts ${chroot_dir}/etc/mtab"

    # make sure this is really existing before bind mounting it
    mkdir ${chroot_dir}/var/lib/nfs

    # Set a default hostname
    echo 'HOSTNAME="ltsp"' > $ROOT/etc/conf.d/hostname
 
    spawn_chroot "rm /etc/init.d/net.eth0"

    cat <<EOF > ${chroot_dir}/etc/lts.conf
# see /usr/share/doc/ltsp-client-$version/lts-parameters.txt.bz2 for a listing 
# of all possible options (Don't forget to include a [default] section)
# TODO: dont serve this file from here
#       put it in /var/lib/tftpboot/ltsp/arch/lts.conf
EOF
}

rcadd ltsp-client-setup boot
rcadd ltsp-client default
