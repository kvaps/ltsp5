#!/bin/bash

# Remove netboot images
[ ! -e /etc/ltsp_chroot ] && exit 0

rm -f /boot/elf-$1.img
rm -f /boot/wraplinux-nbi-$1.img
