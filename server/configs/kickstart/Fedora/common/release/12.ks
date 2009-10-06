%packages --excludedocs
dejavu-sans-fonts
dejavu-sans-mono-fonts
dejavu-serif-fonts
plymouth-theme-charge
dracut-network
# needed for lokkit which is needed by livecd-creator
system-config-firewall-base
%end

%post
/usr/sbin/plymouth-set-default-theme charge
/usr/sbin/ltsp-rewrap-latest-kernel
%end
