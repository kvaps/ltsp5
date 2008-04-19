use_linux32

chroot_dir /chroots/x86

skip partition
skip setup_md_raid
skip setup_lvm
skip format_devices
skip mount_local_partitions
skip mount_network_shares
skip install_bootloader
skip configure_bootloader
#skip build_kernel
genkernel_opts --all-initrd-modules
stage_uri file:///home/johnny/projects/ltsp/stages/stage3-x86-2008.03.17.tar.bz2
tree_type none

pre_install_portage_tree() {
  spawn "mkdir ${chroot_dir}/usr/portage"
  spawn "mount /usr/portage ${chroot_dir}/usr/portage -o bind"

# TODO: set useful use flags
#cat >> ${chroot_dir}/etc/make.conf <<EOF
#USE=""
#PORTDIR_OVERLAY="/usr/local/portage/ltsp"
#EOF


  cat > ${chroot_dir}/etc/fstab <<EOF
#Dynamically replaced on client boot
EOF
  mkdir -p ${chroot_dir}/etc/portage/
  echo "sys-kernel/genkernel" > ${chroot_dir}/etc/portage/package.keywords
  echo "portdbapi.auxdbmodule = cache.metadata_overlay.database" > ${chroot_dir}/etc/portage/modules
}

cron none
pre_install_extra_packages() {
  spawn_chroot "emerge --update --deep world"
}
# TODO: add ltspfs ltsp ldm
#extra_packages xorg-server pulseaudio joystick splashutils

rootpw password
