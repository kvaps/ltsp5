# Kickstart Definition for Client Chroot for i386

# we are going to install into a chroot, such as /opt/ltps/i386
install

# rev #2 will be configurable (i.e. http or ftp or cdrom/dvd or nfs, etc, etc)
#repo --name=released-9 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-10&arch=i386
#repo --name=updates-9 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f10&arch=i386
#repo --name=temporary-9 --baseurl=http://togami.com/~k12linux-temporary/fedora/10/i386/
repo --name=rawhide-10 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=rawhide&arch=i386
repo --name=temporary-10 --baseurl=http://togami.com/~k12linux-temporary/fedora/10/i386/

%include ../common.ks
%end
