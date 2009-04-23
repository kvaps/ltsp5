# Kickstart Definition for Client Chroot for ppc

# we are going to install into a chroot, such as /opt/ltsp/ppc
install

repo --name=rawhide-12-ppc --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=rawhide&arch=ppc
repo --name=temporary-12-ppc --baseurl=http://togami.com/~k12linux-temporary/fedora/12/ppc/

%include ../common/common.ks
%include ../common/arch/ppc.ks
%include ../common/release/12.ks
