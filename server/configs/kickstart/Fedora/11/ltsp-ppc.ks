# Kickstart Definition for Client Chroot for generic ppc

# we are going to install into a chroot, such as /opt/ltsp/ppc
install

repo --name=released-11-ppc --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-11&arch=ppc
repo --name=updates-11-ppc --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f11&arch=ppc
#repo --name=temporary-11-ppc --baseurl=http://togami.com/~k12linux-temporary/fedora/11/ppc/

%include ../common/common.ks
%include ../common/arch/ppc.ks
%include ../common/release/11.ks
