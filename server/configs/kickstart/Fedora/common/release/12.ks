%packages --excludedocs
dejavu-sans-fonts
dejavu-sans-mono-fonts
dejavu-serif-fonts
%end

%post
# Set plymouth graphical boot
ln -sf solar.so /usr/lib/plymouth/default.so
/usr/sbin/ltsp-rewrap-latest-kernel
%end
