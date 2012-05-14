# Kickstart Definition for Client Chroot for i386

# we are going to install into a chroot, such as /opt/ltsp/i386
install

repo --name=released-11-i386 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=fedora-11&arch=i386
repo --name=updates-11-i386 --mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=updates-released-f11&arch=i386
repo --name=temporary-11-i386 --baseurl=http://mplug.org/~k12linux/rpm/f11/i386/

%include ../common/common.ks
%include ../common/arch/i586.ks
%include ../common/release/11.ks

%packages --excludedocs
dracut
dracut-network
%end

