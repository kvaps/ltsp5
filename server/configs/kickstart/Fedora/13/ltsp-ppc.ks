# Kickstart Definition for Client Chroot for ppc

# we are going to install into a chroot, such as /opt/ltsp/ppc
install

repo --name=rawhide-13-ppc --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=rawhide&arch=ppc
#repo --name=temporary-13-ppc --baseurl=http://togami.com/~k12linux-temporary/fedora/13/ppc/

%include ../common/common.ks
%include ../common/arch/ppc.ks
%include ../common/release/13.ks
