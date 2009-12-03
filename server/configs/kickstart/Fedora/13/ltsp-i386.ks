# Kickstart Definition for Client Chroot for i386

# we are going to install into a chroot, such as /opt/ltsp/i386
install

repo --name=rawhide-13-i386 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=rawhide&arch=i386
#repo --name=temporary-13-i386 --baseurl=http://togami.com/~k12linux-temporary/fedora/13/i386/

%include ../common/common.ks
%include ../common/arch/i686.ks
%include ../common/release/13.ks
