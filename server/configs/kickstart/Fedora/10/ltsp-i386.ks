# Kickstart Definition for Client Chroot for i386

# we are going to install into a chroot, such as /opt/ltsp/i386
install

repo --name=released-10-i386 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-10&arch=i386
repo --name=updates-10-i386 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f10&arch=i386
#repo --name=temporary-10-i386 --baseurl=http://togami.com/~k12linux-temporary/fedora/10/i386/

%include ../common/common.ks
%include ../common/arch/i386.ks
%include ../common/release/10.ks

# No i586 clients?  Try this instead.
#%include ../common/arch/i686.ks
