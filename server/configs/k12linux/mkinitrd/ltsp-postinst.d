#!/bin/bash
# Symlink vmlinuz.ltsp and initrd.ltsp and set permissions for tftp server
[ ! -e /etc/ltsp_chroot ] && exit 0

ln -sf vmlinuz-$1    /boot/vmlinuz.ltsp
ln -sf initrd-$1.img /boot/initrd.ltsp
chmod 644 /boot/initrd-$1.img
