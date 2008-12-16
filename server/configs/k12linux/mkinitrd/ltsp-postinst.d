#!/bin/bash

# Prep kernel and initrd for various types of LTSP netboot
[ ! -e /etc/ltsp_chroot ] && exit 0
KERNELOPTS="ro quiet selinux=0 rhgb"

# Image for ELF and coreboot, Etherboot-5.4
# Not using wraplinux for ELF because it cannot boot on coreboot.
if [ -x /usr/sbin/mkelfImage ]; then
  rm -f /boot/elf-$1.img
  mkelfImage --kernel=/boot/vmlinuz-$1 --initrd=/boot/initrd-$1.img --output=/boot/elf-$1.img --append="$KERNELOPTS"
  ln -sf elf-$1.img /boot/elf.ltsp
fi

# Wraplinux NBI
if [ -x /usr/bin/wraplinux ]; then
  rm -f /boot/wraplinux-nbi-$1.img
  wraplinux --nbi /boot/vmlinuz-$1 --initrd /boot/initrd-$1.img -o /boot/wraplinux-nbi-$1.img
  ln -sf wraplinux-nbi-$1.img /boot/wraplinux-nbi.ltsp
fi

# PPC: Copy yaboot into /boot
if [ -e /usr/lib/yaboot/yaboot ]; then
  cp /usr/lib/yaboot/yaboot /boot/yaboot
  chmod 644 /boot/yaboot
fi

# Symlink vmlinuz.ltsp and initrd.ltsp and set permissions for tftp server
ln -sf vmlinuz-$1    /boot/vmlinuz.ltsp
ln -sf initrd-$1.img /boot/initrd.ltsp
chmod 644 /boot/initrd-$1.img
