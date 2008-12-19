# Kickstart Definition for Client Chroot for i686 and generic x86_64

# we are going to install into a chroot, such as /opt/ltps/x86_64
install

repo --name=rawhide-11-x86_64 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=rawhide&arch=x86_64
repo --name=temporary-11-x86_64 --baseurl=http://togami.com/~k12linux-temporary/fedora/11/x86_64/

%include ../common.ks
%include ../common-x86_64.ks
