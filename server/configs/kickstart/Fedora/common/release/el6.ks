%packages --excludedocs
# needed for lspci
pciutils
plymouth-system-theme
dejavu-sans-fonts
dejavu-sans-mono-fonts
dejavu-serif-fonts
dracut-network
# needed for lokkit which is needed by livecd-creator
system-config-firewall-base
%end

%post
/usr/sbin/plymouth-set-default-theme rings
/usr/sbin/ltsp-rewrap-latest-kernel
%end
