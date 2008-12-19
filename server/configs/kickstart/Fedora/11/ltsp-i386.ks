# Kickstart Definition for Client Chroot for i386

# we are going to install into a chroot, such as /opt/ltps/i386
install

repo --name=rawhide-11-i386 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=rawhide&arch=i386
repo --name=temporary-11-i386 --baseurl=http://togami.com/~k12linux-temporary/fedora/11/i386/

%include ../common.ks
%include ../common-i386.ks

# No i586 clients?  Try this instead.
#%include ../common-i686.ks
