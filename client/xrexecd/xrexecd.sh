#!/bin/sh

LOGFILE=/var/log/ldm.log

logit() {
    if [ -n "${LOGFILE}" ]; then
        echo "$1" >> ${LOGFILE}
    else
        echo "$1"
    fi 
}

if [ -n "${LDM_USERNAME}" -a -n "$(/usr/bin/id ${LDM_USERNAME})" ]; then
    true
    #logit "LDM_USERNAME is valid"
else
    logit "Unknown user:  $LDM_USERNAME"
    exit 1
fi

if [ -z "$DISPLAY" ];then 
    logit "Unknown DISPLAY"
    exit 1
fi

# Initialize LTSP_COMMAND as blank
xprop -root -f LTSP_COMMAND 8s -set LTSP_COMMAND ""

# Make sure the local user has access to X
chown ${LDM_USERNAME} $XAUTHORITY

# Poll for LTSP_COMMAND changes and execute
while :; do
    LTSP_COMMAND="$(xprop -root -notype LTSP_COMMAND)"
    [ "$?" != 0 ] && exit 

    LTSP_COMMAND=$(echo "${LTSP_COMMAND}"|sed -e 's/^LTSP_COMMAND = //' -e 's/^"//' -e 's/"$//')
    if [ -n "${LTSP_COMMAND}" ]; then
        logit "Executing the following command as ${LDM_USERNAME}: ${LTSP_COMMAND} "

        su - ${LDM_USERNAME} -c "XAUTHORITY=$XAUTHORITY ${LTSP_COMMAND}" &

        xprop -root -f LTSP_COMMAND 8s -set LTSP_COMMAND ""
    fi
    sleep 1
done
