# Kickstart Definition for Client Chroot for i686

# we are going to install into a chroot, such as /opt/ltsp/i386
install

repo --name=sl6-os-i686         --baseurl=http://chloe/scientific/6.0/i386/os/
repo --name=sl6-fastbugs-i686   --baseurl=http://chloe/scientific/6.0/i386/updates/fastbugs/
repo --name=sl6-security-i686   --baseurl=http://chloe/scientific/6.0/i386/updates/security/
repo --name=temporary-el6-i686  --baseurl=http://k12linux-temp/~k12linux/rpm/el6/i686/

%include ../common/common.ks
%include ../common/arch/i686.ks
%include ../common/release/el6.ks
