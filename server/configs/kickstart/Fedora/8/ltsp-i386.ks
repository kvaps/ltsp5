# Kickstart Definition for Client Chroot for i386

# we are going to install into a chroot, such as /opt/ltps/i386
install

# rev #2 will be configurable (i.e. http or ftp or cdrom/dvd or nfs, etc, etc)
repo --name=released --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-8&arch=i386
repo --name=updates --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f8&arch=i386
repo --name=temporary --baseurl=http://togami.com/~k12linux-temporary/fedora/8/i386/
repo --name=client-8  --baseurl=http://togami.com/~k12linux-temporary/fedora/8-client/i386/

%include ../common.ks
%end
