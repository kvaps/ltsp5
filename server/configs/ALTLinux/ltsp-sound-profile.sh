X_SERVER=${DISPLAY%:*}
if [ -n "$X_SERVER" -a "$X_SERVER" != "localhost" ]; then
    if [ -x /usr/bin/ltspinfo ]; then
	SAVE_IFS="$IFS"
	IFS=''
	eval `/usr/bin/ltspinfo -h $X_SERVER -c all 2>/dev/null`
	IFS="$SAVE_IFS"
	unset SAVE_IFS
	if [ -n "$LTSP_AUDIO_SERVER" ]; then
	    case "$LTSP_AUDIO_SERVER" in
		NAS) export AUDIOSERVER=tcp/$X_SERVER:${LTSP_AUDIO_PORT:-8000} ;;
		ESOUND|ESD) export ESPEAKER=$X_SERVER:${LTSP_AUDIO_PORT:-16001} ;;
		PULSE|PULSEAUDIO)
		    export PULSE_SERVER=$X_SERVER:${LTSP_AUDIO_PORT:-4713}
		    export ESPEAKER=$X_SERVER:${LTSP_AUDIO_PORT:-16001}
		    ;;
	    esac
	fi
	unset LTSP_AUDIO_SERVER LTSP_AUDIO_PORT
    else
	export ESPEAKER="$X_SERVER:16001"
	export AUDIOSERVER="tcp/$X_SERVER:8000"
	#export PULSE_SERVER="tcp:$X_SERVER:4713"
    fi
    if [ -n "$PULSE_SERVER" -a -f /etc/ltsp/asound-pulse.conf -a -f /usr/share/ltsp/alsa-pulse.conf -a -f /usr/lib/alsa-lib/l
	export ALSA_CONFIG_PATH=/usr/share/ltsp/alsa-pulse.conf
    elif [ -f /etc/ltsp/asound-null.conf ]; then
	export -rx ALSA_CONFIG_PATH=/usr/share/ltsp/alsa-null.conf
    fi                                                f
fi
unset X_SERVER
