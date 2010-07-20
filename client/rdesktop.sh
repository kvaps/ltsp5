#!/bin/sh
# we need this to be able to start rdesktop with pasuspender

PATH=/bin:$PATH; export PATH
. /usr/share/ltsp/screen-x-common

unset prefix
case "${RDP_SOUND}" in
    nopulse)
        [ -x "/usr/bin/pasuspender" ] && prefix="/usr/bin/pasuspender --"
        # Only modify RDP_OPTIONS if no options are passed with the command
        # This way, individual rdesktop screens can turn off sound
        # with a call to: SCREEN_XX="rdesktop -r sound:none"
        [ $# -lt 2 ] && RDP_OPTIONS="${RDP_OPTIONS} -r sound:local:oss"
        ;;
    pulse-oss)
        [ -x "/usr/bin/padsp" ] && prefix="/usr/bin/padsp"
        # Only modify RDP_OPTIONS if no options are passed with the command
        # This way, individual rdesktop screens can turn off sound
        # with a call to: SCREEN_XX="rdesktop -r sound:none"
        [ $# -lt 2 ] && RDP_OPTIONS="${RDP_OPTIONS} -r sound:local:oss"
        ;;
esac

RDESKTOP_OPTIONS="-f -u '' ${RDP_OPTIONS} $* ${RDP_SERVER}"

eval $prefix /usr/bin/rdesktop ${RDESKTOP_OPTIONS}
