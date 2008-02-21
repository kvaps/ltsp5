Name:           ltsp
Version:        REPLACE_VERSION_HERE
Release:        unofficial.REPLACE_DATE_HERE%{?dist}
Summary:        LTSP

Group:          User Interface/Desktops
License:        GPL+
URL:            http://www.ltsp.org
Source0:        ltsp-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: popt-devel
BuildRequires: flex bison
BuildRequires: automake
BuildRequires: libX11-devel
%ifarch i386 x86_64
# Need pxelinux.0 from syslinux if server is x86
BuildRequires: syslinux
# Need location of tftpboot directory from tftp-server
BuildRequires: tftp-server
%define _tftpdir %(cat /etc/xinetd.d/tftp |grep server_args | awk -F"-s " {'print $2'} || echo -n "/BOGUS/DIRECTORY")
%endif

%package client
Summary:        LTSP client
Group:          User Interface/Desktops
Requires:       chkconfig

%package server
Summary:        LTSP server
Group:          User Interface/Desktops
# needed to install client chroots
Requires:       livecd-tools

%description
LTSP client and server

%description client
LTSP client

%description server
LTSP server

%prep
%setup -q 

%build
pushd client/getltscfg
  make %{?_smp_mflags}
popd

pushd client/xrexecd
  ./autogen.sh
  %configure
  make %{?_smp_mflags}
popd


%install
##### make directories
rm -rf $RPM_BUILD_ROOT
# client
mkdir -p $RPM_BUILD_ROOT%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man1/
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ltsp/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/init.d/
mkdir -p $RPM_BUILD_ROOT%{_prefix}/lib/ltsp/

# server
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man8
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/kickstart/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/init.d/
mkdir -p $RPM_BUILD_ROOT%{_sbindir}
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ltsp/scripts/
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ltsp/plugins/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/cron.daily/
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/lib/ltsp/swapfiles/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d/

%ifarch i386 x86_64
mkdir -p $RPM_BUILD_ROOT%{_tftpdir}/ltsp/i386/pxelinux.cfg/
mkdir -p $RPM_BUILD_ROOT%{_tftpdir}/ltsp/x86_64/pxelinux.cfg/
%endif
mkdir -p $RPM_BUILD_ROOT%{_tftpdir}/ltsp/ppc/
mkdir -p $RPM_BUILD_ROOT%{_tftpdir}/ltsp/ppc64/

###### client install
pushd client/xrexecd
    make install DESTDIR=$RPM_BUILD_ROOT
popd

install -m 0755 client/getltscfg/getltscfg $RPM_BUILD_ROOT/%{_bindir}/getltscfg
install -m 0644 client/getltscfg/getltscfg.1 $RPM_BUILD_ROOT/%{_mandir}/man1/
install -m 0644 client/ltsp_config $RPM_BUILD_ROOT/%{_prefix}/lib/ltsp/
install -m 0755 client/screen_session $RPM_BUILD_ROOT/%{_prefix}/lib/ltsp/
install -m 0755 client/configure-x.sh $RPM_BUILD_ROOT/%{_prefix}/lib/ltsp/
install -m 0644 client/initscripts/ltsp-init-common $RPM_BUILD_ROOT/%{_prefix}/lib/ltsp/
install -m 0755 client/initscripts/RPM/ltsp-client-launch $RPM_BUILD_ROOT%{_sysconfdir}/init.d/
cp -av client/screen.d $RPM_BUILD_ROOT/%{_prefix}/lib/ltsp/

# fedora-specific scripts
#mkdir -p $RPM_BUILD_ROOT/sbin/
#install -m 0755 %{SOURCE2} ${RPM_BUILD_ROOT}/sbin/mkinitrd-ltsp
#install -m 0755 %{SOURCE3} ${RPM_BUILD_ROOT}/sbin/ltsp-client-configure

### server install
install -m 0755 server/nbdrootd $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/nbdswapd $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ldminfod $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ltsp-update-sshkeys $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ltsp-build-client $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ltsp-update-kernels $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/scripts/chroot-creator $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ltsp-swapfile-delete $RPM_BUILD_ROOT%{_sysconfdir}/cron.daily/
install -m 0644 server/xinetd.d/nbdrootd $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d/
install -m 0644 server/xinetd.d/nbdswapd $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d/
install -m 0644 server/xinetd.d/ldminfod $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d/
install -m 0644 server/configs/nbdswapd.conf $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/
cp -pr server/configs/kickstart/* $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/kickstart/
cp -pr server/plugins/* $RPM_BUILD_ROOT%{_datadir}/ltsp/plugins/

install -m 0644 server/configs/dhcpd-k12linux.conf $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/ltsp-dhcpd.conf
install -m 0755 server/services/ltsp-dhcpd.init $RPM_BUILD_ROOT%{_sysconfdir}/init.d/ltsp-dhcpd

%ifarch i386 x86_64
install -m 0644 server/configs/pxe-default.conf $RPM_BUILD_ROOT%{_tftpdir}/ltsp/i386/pxelinux.cfg/default
install -m 0644 server/configs/pxe-default.conf $RPM_BUILD_ROOT%{_tftpdir}/ltsp/x86_64/pxelinux.cfg/default
install -m 0644 /usr/lib/syslinux/pxelinux.0 $RPM_BUILD_ROOT%{_tftpdir}/ltsp/i386
install -m 0644 /usr/lib/syslinux/pxelinux.0 $RPM_BUILD_ROOT%{_tftpdir}/ltsp/x86_64
%endif

touch $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/ltsp-build-client.conf

##SKIPPED:
#/etc/network/if-up.d/ltsp-keys
#/usr/sbin/ltsp-update-image

%clean
rm -rf $RPM_BUILD_ROOT

%post client
/sbin/chkconfig --add ltsp-client-launch
exit 0

%preun
if [ $1 = 0 ] ; then
    /sbin/chkconfig --del ltsp-client-launch
fi
exit 0


%files client
%defattr(-,root,root,-)
%{_mandir}/man1/getltscfg.1.gz
%{_bindir}/getltscfg
%{_bindir}/xrexecd
#/sbin/ltsp-client-configure
#/sbin/mkinitrd-ltsp
%{_prefix}/lib/ltsp
%{_sysconfdir}/init.d/ltsp-client-launch


%files server
%defattr(-,root,root,-)
%doc ChangeLog COPYING TODO
%{_localstatedir}/lib/ltsp/swapfiles/
%dir %{_tftpdir}/
%dir %{_tftpdir}/ltsp/
%dir %{_tftpdir}/ltsp/i386/
%dir %{_tftpdir}/ltsp/i386/pxelinux.cfg/
%dir %{_tftpdir}/ltsp/x86_64/
%dir %{_tftpdir}/ltsp/x86_64/pxelinux.cfg/
%dir %{_tftpdir}/ltsp/ppc/
%dir %{_tftpdir}/ltsp/ppc64/
%ifarch i386 x86_64
%{_tftpdir}/ltsp/i386/pxelinux.0
%{_tftpdir}/ltsp/x86_64/pxelinux.0
%config(noreplace) %{_tftpdir}/ltsp/i386/pxelinux.cfg/default
%config(noreplace) %{_tftpdir}/ltsp/x86_64/pxelinux.cfg/default
%endif

%{_sbindir}/ltsp-build-client
%{_sbindir}/ltsp-update-kernels
%{_datadir}/ltsp/scripts/
%{_datadir}/ltsp/plugins/
%{_sbindir}/ldminfod
%{_sbindir}/ltsp-update-sshkeys
%{_sbindir}/nbdrootd
%{_sbindir}/nbdswapd
%{_sbindir}/chroot-creator
%{_sysconfdir}/cron.daily/ltsp-swapfile-delete
%{_sysconfdir}/xinetd.d/nbdrootd
%{_sysconfdir}/xinetd.d/nbdswapd
%{_sysconfdir}/xinetd.d/ldminfod
%{_sysconfdir}/init.d/ltsp-dhcpd
# Configuration Files
%dir %{_sysconfdir}/ltsp/
%config(noreplace) %{_sysconfdir}/ltsp/ltsp-build-client.conf
%config(noreplace) %{_sysconfdir}/ltsp/nbdswapd.conf 
%config(noreplace) %{_sysconfdir}/ltsp/ltsp-dhcpd.conf
%dir %{_sysconfdir}/ltsp/kickstart/
%dir %{_sysconfdir}/ltsp/kickstart/Fedora/
%dir %{_sysconfdir}/ltsp/kickstart/Fedora/8/
%dir %{_sysconfdir}/ltsp/kickstart/Fedora/9/
%config(noreplace) %{_sysconfdir}/ltsp/kickstart/*/*/*.ks

#K12 stuff
#/usr/sbin/ltsp-initialize
#/usr/share/ltsp/chkconfig.d
#/usr/share/ltsp/scripts.d
#%config(noreplace) /etc/ltsp/ltsp.conf

%changelog

