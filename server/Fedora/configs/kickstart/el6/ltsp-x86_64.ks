# Kickstart Definition for Client Chroot for x86_64

# we are going to install into a chroot, such as /opt/ltsp/x86_64
install

repo --name=sl6-os-x86_64         --baseurl=http://mirror.ancl.hawaii.edu/linux/scientific/6.1/x86_64/os/
repo --name=sl6-fastbugs-x86_64   --baseurl=http://mirror.ancl.hawaii.edu/linux/scientific/6.1/x86_64/updates/fastbugs/
repo --name=sl6-security-x86_64   --baseurl=http://mirror.ancl.hawaii.edu/linux/scientific/6.1/x86_64/updates/security/
repo --name=epel6-x86_64          --mirrorlist=https://mirrors.fedoraproject.org/metalink?repo=epel-6&arch=x86_64
repo --name=temporary-el6-x86_64  --baseurl=http://mplug.org/~k12linux/rpm/el6/x86_64/

%include ../common/common.ks
%include ../common/arch/x86_64.ks
%include ../common/release/el6.ks
