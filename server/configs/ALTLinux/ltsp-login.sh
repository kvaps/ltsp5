#!/bin/sh

. gettext.sh

x_login_num()
{
    who | egrep -c "^${USER}[[:blank:]]+[[:graph:]]*:"
}

DIALOG=
TIMEOUT=30

. /usr/share/ltsp/ltsp-server-functions

UVer()
{
    echo $((${1:-0} * 256 * 256 + ${2:-0} * 256 + ${3:-0}))
}

XDVer()
{
    local MAJOR MINOR MICRO
    Xdialog --version 2>&1 | (IFS=. read MAJOR MINOR MICRO; UVer $MAJOR $MINOR)
}

Error()
{
    local saveLANG
    if [ "$DIALOG" = "kdialog" ]; then
	kdialog --title "$(eval_gettext "Login Error")" \
	    --error "$(eval_gettext "User") \"${USER}\" $(eval_gettext "already logged in"\!)"
    elif [ "$DIALOG" = "zenity" ]; then
	zenity --title="$(eval_gettext "Login Error")" --error \
	    --text="$(eval_gettext "User") \"${USER}\" $(eval_gettext "already logged in"\!)\n\n$(eval_gettext "Continue")?"
    elif [ "$DIALOG" = "Xdialog" ]; then
	if [ $(XDVer) -ge $(UVer 2 3) ]; then
	    saveLANG="$LANG"
	    LANG=${LANG%%.*}.UTF-8
	fi
	Xdialog --title "$(eval_gettext "Login Error")" \
	    --center \
	    --beep \
	    --ok-label "$(eval_gettext "Continue")" \
	    --infobox "$(eval_gettext "User") \"${USER}\" $(eval_gettext "already logged in"\!)" 10 50 $((${TIMEOUT:-0} * 1000))
	[ -z "$saveLANG" ] || LANG="$saveLANG"
    elif [ "$DIALOG" = "wish" ]; then
	echo "\
	    wm withdraw .
	    tk_messageBox \
		-title \"$(eval_gettext "Login Error")\" \
		-icon error \
		-message \"$(eval_gettext "User") \\\"${USER}\\\" $(eval_gettext "already logged in"\!)\" \
		-type ok
		exit 2" | wish
    elif [ "$DIALOG" = "gxmessage" ]; then
	$DIALOG -title "Login Error" \
	    -geometry 360x80 \
	    -center \
	    -buttons Cancel:2 \
	    -default Cancel \
	    -timeout ${TIMEOUT:-0} \
	    "$(eval_gettext "User") \"${USER}\" $(eval_gettext "already logged in"\!)"
    elif [ "$DIALOG" = "xmessage" ]; then
	$DIALOG -title "Login Error" \
	    -geometry 360x80 \
	    -center \
	    -buttons Cancel:2 \
	    -default Cancel \
	    -timeout ${TIMEOUT:-0} \
	    "User \"${USER}\" already logged in"\!
    else
	return 2
    fi
}

Warning()
{
    local saveLANG RETVAL
    if [ "$DIALOG" = "kdialog" ]; then
	kdialog --title "$(eval_gettext "Login Warning")" \
	    --warningcontinuecancel "$(eval_gettext "User") \"${USER}\" $(eval_gettext "already logged in"\!)"
    elif [ "$DIALOG" = "zenity" ]; then
	zenity --title="$(eval_gettext "Login Warning")" --warning \
	    --text="$(eval_gettext "User") \"${USER}\" $(eval_gettext "already logged in"\!)\n\n$(eval_gettext "Continue")?"
    elif [ "$DIALOG" = "Xdialog" ]; then
	if [ $(XDVer) -ge $(UVer 2 3) ]; then
	    saveLANG="$LANG"
	    LANG=${LANG%%.*}.UTF-8
	fi
	Xdialog --title "$(eval_gettext "Login Warning")" \
	    --center \
	    --beep \
	    --ok-label "$(eval_gettext "Continue")" \
	    --cancel-label "$(eval_gettext "Cancel")" \
	    --yesno "$(eval_gettext "User") \"${USER}\" $(eval_gettext "already logged in"\!)" 10 50
	RETVAL=$?
	[ -z "$saveLANG" ] || LANG="$saveLANG"
	return RETVAL
   elif [ "$DIALOG" = "wish" ]; then
	echo "\
	    wm withdraw .
	    if {[tk_messageBox \
		-title \"$(eval_gettext "Login Warning")\" \
		-icon warning \
		-message \"$(eval_gettext "User") \\\"${USER}\\\" $(eval_gettext "already logged in"\!)\" \
		-type okcancel \
		-default cancel] == \"ok\"} then exit \
	    else {exit 2}" | wish
    elif [ "$DIALOG" = "gxmessage" ]; then
	$DIALOG -title "Login Warning" \
	    -geometry 360x120 \
	    -center \
	    -buttons OK:0,Cancel:2 \
	    -default Cancel \
	    "$(eval_gettext "User") \"${USER}\" $(eval_gettext "already logged in"\!)
	    
	    $(eval_gettext "Continue")?"
    elif [ "$DIALOG" = "xmessage" ]; then
	$DIALOG -title "Login Warning" \
	    -geometry 300x120 \
	    -center \
	    -buttons OK:0,Cancel:2 \
	    -default Cancel \
	    "User \"${USER}\" already logged in"\!"
	    
	    Continue?"
    else
	return 0
    fi
}

SourceIfNotEmpty /etc/sysconfig/xinitrc ||:

X_MULTI_LOGIN=${X_MULTI_LOGIN:-W}

if ! is_yes "$X_MULTI_LOGIN"; then
    export TEXTDOMAIN=ltsp-login
    export TEXTDOMAINDIR=/usr/share/locale
    if [ $(x_login_num) -gt 1 ]; then
	for D in kdialog zenity Xdialog wish gxmessage xmessage; do
	    if which $D >/dev/null 2>&1; then
		DIALOG="$D"
		break
	    fi
	done
	DIALOG=${DIALOG:-none}
	if is_no "$X_MULTI_LOGIN"; then
	    Error
	    exit 2
	else
	    Warning || exit 0
	fi
    fi
    unset TEXTDOMAIN TEXTDOMAINDIR D
fi
unset DIALOG TIMEOUT
