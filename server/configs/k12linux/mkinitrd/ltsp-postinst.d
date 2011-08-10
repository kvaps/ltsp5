#!/bin/bash

# Skip first time (optimization)
if [ -f /etc/dracut.conf.d/skip-first-time.conf ]; then
    rm -f /etc/dracut.conf.d/skip-first-time.conf
    exit 0
fi

if [ -f /boot/initramfs-$1.img ]; then
    INITRD=initramfs-$1.img
    rm -f /boot/initrd-$1.img
else
    INITRD=initrd-$1.img
fi

# Prep kernel and initrd for various types of LTSP netboot
[ ! -e /etc/ltsp_chroot ] && exit 0
KERNELOPTS="ro quiet selinux=0 rhgb"

# Image for ELF and coreboot, Etherboot-5.4
# Not using wraplinux for ELF because it cannot boot on coreboot.
if [ -x /usr/sbin/mkelfImage ]; then
  rm -f /boot/elf-$1.img
  /usr/sbin/mkelfImage --kernel=/boot/vmlinuz-$1 --initrd=/boot/$INITRD --output=/boot/elf-$1.img --append="$KERNELOPTS"
  ln -sf elf-$1.img /boot/elf.ltsp
fi

# Wraplinux NBI
if [ -x /usr/bin/wraplinux ]; then
  rm -f /boot/wraplinux-nbi-$1.img
  /usr/bin/wraplinux --nbi /boot/vmlinuz-$1 --initrd /boot/$INITRD -o /boot/wraplinux-nbi-$1.img
  ln -sf wraplinux-nbi-$1.img /boot/wraplinux-nbi.ltsp
fi

# PPC: Copy yaboot into /boot
if [ -e /usr/lib/yaboot/yaboot ]; then
  cp /usr/lib/yaboot/yaboot /boot/yaboot
  chmod 644 /boot/yaboot
fi

# SPARC: Convert ELF to AOUT for OFW netboot, and use piggyback to add System.map and initrd to the image
if [ -x /usr/bin/elftoaout ]; then
  elftoaout /boot/vmlinuz-$1 -o /boot/aout-$1
fi
if [ -x /usr/bin/piggyback64 ]; then
  if file /boot/vmlinuz-$1 |grep -q "ELF 64-bit"; then
    PIGGY=/usr/bin/piggyback64
  else
    PIGGY=/usr/bin/piggyback
  fi
  $PIGGY /boot/aout-$1 /boot/System.map-$1 /boot/$INITRD
fi

# Symlink vmlinuz.ltsp and initrd.ltsp and set permissions for tftp server
ln -sf vmlinuz-$1    /boot/vmlinuz.ltsp
ln -sf $INITRD /boot/initrd.ltsp
chmod 644 /boot/$INITRD
