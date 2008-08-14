#!/bin/bash

# Prep kernel and initrd for various types of LTSP netboot
[ ! -e /etc/ltsp_chroot ] && exit 0
KERNELOPTS="ro quiet selinux=0 rhgb"

# Image for ELF and coreboot, Etherboot-5.4
if [ -x /usr/sbin/mkelfImage ]; then
  rm -f /boot/elf-$1.img
  mkelfImage --kernel=/boot/vmlinuz-$1 --initrd=/boot/initrd-$1.img --output=/boot/elf-$1.img --append="$KERNELOPTS"
  ln -sf elf-$1.img /boot/elf.ltsp
fi

# Image for really old NBI-only clients
if [ -x /usr/bin/mknbi-linux ]; then
  rm -f /boot/legacy-nbi-$1.img
  mknbi-linux /boot/vmlinuz-$1 /boot/initrd-$1.img --output=/boot/legacy-nbi-$1.img --append="$KERNELOPTS" 2> /dev/null
  ln -sf legacy-nbi-$1.img /boot/legacy-nbi.ltsp
fi

# Wraplinux NBI
if [ -x /usr/bin/wraplinux ]; then
  rm -f /boot/wraplinux-nbi-$1.img
  wraplinux --nbi /boot/vmlinuz-$1 --initrd /boot/initrd-$1.img -o /boot/wraplinux-nbi-$1.img
  ln -sf wraplinux-nbi-$1.img /boot/wraplinux-nbi.ltsp
fi

# Wraplinux ELF
if [ -x /usr/bin/wraplinux ]; then
  rm -f /boot/wraplinux-elf-$1.img
  wraplinux --elf /boot/vmlinuz-$1 --initrd /boot/initrd-$1.img -o /boot/wraplinux-elf-$1.img
  ln -sf wraplinux-elf-$1.img /boot/wraplinux-elf.ltsp
fi

# Symlink vmlinuz.ltsp and initrd.ltsp and set permissions for tftp server
ln -sf vmlinuz-$1    /boot/vmlinuz.ltsp
ln -sf initrd-$1.img /boot/initrd.ltsp
chmod 644 /boot/initrd-$1.img
