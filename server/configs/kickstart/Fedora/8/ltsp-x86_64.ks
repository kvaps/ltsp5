# Kickstart Definition for Client Chroot for x86_64

# we are going to install into a chroot, such as /opt/ltps/x86_64
install

# rev #2 will be configurable (i.e. http or ftp or cdrom/dvd or nfs, etc, etc)
repo --name=released-8-x86_64 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-8&arch=x86_64
repo --name=updates-8-x86_64 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f8.newkey&arch=x86_64
repo --name=temporary-8-x86_64 --baseurl=http://togami.com/~k12linux-temporary/fedora/8/x86_64/
repo --name=temporary-client-8-x86_64  --baseurl=http://togami.com/~k12linux-temporary/fedora/8-client/x86_64/

%include ../common.ks
%include ../common-x86_64.ks
