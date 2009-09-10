%packages --excludedocs
dejavu-sans-fonts
dejavu-sans-mono-fonts
dejavu-serif-fonts
plymouth-theme-charge
%end

%post
/usr/sbin/plymouth-set-default-theme charge
/usr/sbin/ltsp-rewrap-latest-kernel
%end
