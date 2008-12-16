# Kickstart Definition for Client Chroot for generic ppc

# we are going to install into a chroot, such as /opt/ltps/ppc
install

# rev #2 will be configurable (i.e. http or ftp or cdrom/dvd or nfs, etc, etc)
repo --name=released-10-ppc --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-10&arch=ppc
repo --name=updates-10-ppc --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f10&arch=ppc
repo --name=temporary-10-ppc --baseurl=http://togami.com/~k12linux-temporary/fedora/10/ppc/

%include ../common.ks
%include ../common-ppc.ks
