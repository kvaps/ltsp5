# Kickstart Definition for Client Chroot for i686 and generic x86_64

# we are going to install into a chroot, such as /opt/ltsp/x86_64
install

repo --name=released-10-x86_64 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-10&arch=x86_64
repo --name=updates-10-x86_64 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f10&arch=x86_64
repo --name=temporary-10-x86_64 --baseurl=http://togami.com/~k12linux-temporary/fedora/10/x86_64/

%include ../common/common.ks
%include ../common/arch/x86_64.ks
%include ../common/release/10.ks
