%include livecd-fedora-9-desktop.ks

%packages
ltsp-server
ltsp-vmclient
ltsp-server-livesetupdocs
%end

%post
cp /etc/ltsp/ltsp-build-client.conf /etc/ltsp/ltsp-build-client.conf.backup
echo "option_cache_value=/var/cache/yum" > /etc/ltsp/ltsp-build-client.conf
/usr/sbin/ltsp-build-client
# clean up
mv /etc/ltsp/ltsp-build-client.conf.backup /etc/ltsp/ltsp-build-client.conf
rm -f /etc/resolv.conf
touch /etc/resolv.conf

# Setup LTSP server
echo "/opt/ltsp *(ro,async,no_root_squash)" > /etc/exports
%end
