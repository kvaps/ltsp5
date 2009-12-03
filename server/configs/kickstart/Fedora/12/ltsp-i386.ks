# Kickstart Definition for Client Chroot for i386

# we are going to install into a chroot, such as /opt/ltsp/i386
install

repo --name=released-12-i386 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-12&arch=i386
repo --name=updates-12-i386 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f12&arch=i386
#repo --name=temporary-12-i386 --baseurl=http://togami.com/~k12linux-temporary/fedora/12/i386/

%include ../common/common.ks
%include ../common/arch/i686.ks
%include ../common/release/12.ks
