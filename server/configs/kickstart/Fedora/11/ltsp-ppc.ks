# Kickstart Definition for Client Chroot for ppc

# we are going to install into a chroot, such as /opt/ltps/ppc
install

repo --name=rawhide-11-ppc --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=rawhide&arch=ppc
repo --name=temporary-11-ppc --baseurl=http://togami.com/~k12linux-temporary/fedora/11/ppc/

%include ../common.ks
%include ../common-ppc.ks
