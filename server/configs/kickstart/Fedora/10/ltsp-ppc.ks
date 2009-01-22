# Kickstart Definition for Client Chroot for generic ppc

# we are going to install into a chroot, such as /opt/ltsp/ppc
install

repo --name=released-10-ppc --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-10&arch=ppc
repo --name=updates-10-ppc --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f10&arch=ppc
repo --name=temporary-10-ppc --baseurl=http://togami.com/~k12linux-temporary/fedora/10/ppc/

%include ../common/common.ks
%include ../common/arch/ppc.ks
%include ../common/release/10.ks
