# Kickstart Definition for Client Chroot for i686 and generic x86_64

# we are going to install into a chroot, such as /opt/ltsp/x86_64
install

repo --name=released-14-x86_64 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-14&arch=x86_64
repo --name=updates-14-x86_64 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f14&arch=x86_64
repo --name=temporary-14-x86_64 --baseurl=http://k12linux-temp/~k12linux/rpm/f14/x86_64/

%include ../common/common.ks
%include ../common/arch/x86_64.ks
%include ../common/release/14.ks
