#!/bin/sh
# -*- mode: shell-script; indent-tabs-mode: nil; sh-basic-offset: 4; -*-
# ex: ts=8 sw=4 sts=4 et filetype=sh

# "mount --move /sysroot /..." doesn't work on rhel7 and nfs root.
# Just create a copy of network config here and do all the magic
# in init-ltsp.d/00-overlay instead.

mount -t tmpfs tmpfs /sysroot/tmp
cp -a /tmp/net.* /tmp/dhclient.* /sysroot/tmp
