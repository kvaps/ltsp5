# Kickstart Definition for Client Chroot for i686 and generic x86_64

# we are going to install into a chroot, such as /opt/ltsp/x86_64
install

repo --name=released-12-x86_64 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-12&arch=x86_64
repo --name=updates-12-x86_64 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f12&arch=x86_64
#repo --name=temporary-12-x86_64 --baseurl=http://togami.com/~k12linux-temporary/fedora/12/x86_64/

%include ../common/common.ks
%include ../common/arch/x86_64.ks
%include ../common/release/12.ks
