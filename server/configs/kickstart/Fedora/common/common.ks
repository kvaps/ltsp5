### Include this from other .ks files

# this is just garbage, it is not used, but if left empty the user is prompted
rootpw --iscrypted $1$7RBvKHQ2$gozxTbUdO9.xBncKZQ9760

# should be selectable...
lang en_US.UTF-8
keyboard us
firewall --enabled --port=22:tcp
network --bootproto=dhcp --device=eth0
authconfig --enableshadow --enablemd5
selinux --disabled
timezone --utc America/Los_Angeles

# cookie-cutter stuff from here
bootloader --location=none
reboot

# this could probably be slimmed-down quite a bit
%packages --excludedocs
k12linux-release
ltsp-client
ltspfsd
ldm
alsa-utils
alsa-plugins-pulseaudio
aspell
aspell-en
atk
audit-libs
audit-libs-python
basesystem
bash
bind-libs
bind-utils
bitmap-fonts
bzip2-libs
cairo
chkconfig
coreutils
cpio
cpp
cracklib
cracklib-dicts
cups-libs
cyrus-sasl-lib
db4
dbus
device-mapper
diffutils
dmraid
e2fsprogs
e2fsprogs-libs
pulseaudio-esound-compat
elfutils-libelf
ethtool
expat
filesystem
findutils
fontconfig
freetype
fuse-sshfs
gawk
gdbm
glib2
glibc-common
gnutls
#gphoto2
grep
gzip
hpijs
hwdata
info
initscripts
iproute
iputils
kpartx
krb5-libs
#kudzu
less
libacl
libattr
libcap
libdmx
libdrm
libexif
libfontenc
#libFS
libgcc
libgcrypt
libgpg-error
libICE
libieee1284
libjpeg
libpng
libsane-hpaio
libselinux
libselinux-python
libsemanage
libsepol
libSM
libstdc++
libtiff
libusb
libuser
libX11
libXau
libXaw
libXdmcp
libXext
libXfont
libXft
libXi
libXinerama
libxkbfile
libxml2
libxml2-python
libXmu
libXpm
libXrandr
libXrender
libXt
libXtst
libXv
libXxf86dga
libXxf86misc
libXxf86vm
lockdev
lvm2
MAKEDEV
mcstrans
mesa-libGL
mingetty
module-init-tools
nbd
nc
ncurses
neon
net-snmp-libs
net-tools
openldap
openssh-clients
pam
passwd
pcre
perl
popt
rpcbind
plymouth-plugin-solar
procps
psmisc
pulseaudio-utils
pulseaudio-module-x11
python
#python-sqlite2
python-urlgrabber
readline
rpm
rpm-libs
rpm-python
sane-backends
sane-backends-libs
sed
setup
shadow-utils
sqlite
system-release
rsyslog
#system-config-display
system-config-firewall-tui
tar
tftp
time
ttmkfdir
tzdata
udev
util-linux-ng
which
xkeyboard-config
xorg-x11-drivers
xorg-x11-fonts-100dpi
xorg-x11-server-utils
xorg-x11-server-Xorg
xorg-x11-xauth
#xorg-x11-xfs
xorg-x11-xkb-utils
xorg-x11-xinit
xterm
ypbind
yp-tools
yum
yum-metadata-parser
zlib
%end
