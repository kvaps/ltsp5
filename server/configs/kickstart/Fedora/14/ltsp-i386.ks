# Kickstart Definition for Client Chroot for i686

# we are going to install into a chroot, such as /opt/ltsp/i386
install

repo --name=released-14-i686 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-14&arch=i386
repo --name=updates-14-i686 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f14&arch=i386
repo --name=temporary-14-i686 --baseurl=http://mplug.org/~k12linux/rpm/f14/i686/

%include ../common/common.ks
%include ../common/arch/i686.ks
%include ../common/release/14.ks
