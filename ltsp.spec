Name:           ltsp
Version:        REPLACE_VERSION_HERE
Release:        unofficial%{?dist}
Summary:        LTSP

Group:          User Interface/Desktops
License:        GPL+
URL:            http://www.ltsp.org
Source0:        ltsp-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: popt-devel
BuildRequires: flex bison

%package client
Summary:        LTSP client
Group:          User Interface/Desktops

%package server
Summary:        LTSP server
Group:          User Interface/Desktops

%description
LTSP

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
# server
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man8
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/X11/Xsession.d/
mkdir -p $RPM_BUILD_ROOT%{_sbindir}
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ltsp/scripts/
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ltsp/plugins/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/cron.daily/
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/lib/tftpboot/ltsp/i386/pxelinux.cfg/
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/lib/ltsp/swapfiles/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d/

###### client install
pushd client/xrexecd
    make install DESTDIR=$RPM_BUILD_ROOT
popd

install -m 0755 client/getltscfg/getltscfg $RPM_BUILD_ROOT/%{_bindir}/getltscfg
install -m 0644 client/getltscfg/getltscfg.1 $RPM_BUILD_ROOT/%{_mandir}/man1/
install -m 0644 client/ltsp_config $RPM_BUILD_ROOT%{_datadir}/ltsp/
install -m 0755 client/screen_session $RPM_BUILD_ROOT%{_datadir}/ltsp/
install -m 0755 client/configure-x.sh $RPM_BUILD_ROOT%{_datadir}/ltsp/
cp -av client/screen.d $RPM_BUILD_ROOT%{_datadir}/ltsp/

# fedora-specific scripts
#mkdir -p $RPM_BUILD_ROOT/sbin/
#install -m 0755 %{SOURCE2} ${RPM_BUILD_ROOT}/sbin/mkinitrd-ltsp
#install -m 0755 %{SOURCE3} ${RPM_BUILD_ROOT}/sbin/ltsp-client-configure

# fedora init scripts
#pushd $RPM_BUILD_ROOT
#   tar xvzf %{SOURCE1}
#popd

### server install
install -m 0755 server/80_ltsp-sound $RPM_BUILD_ROOT%{_sysconfdir}/X11/Xsession.d/
install -m 0755 server/nbdrootd $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ldminfod $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ltsp-update-sshkeys $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/nbdswapd $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ltsp-build-client $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ltsp-update-kernels $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ltsp-swapfile-delete $RPM_BUILD_ROOT%{_sysconfdir}/cron.daily/
install -m 0644 server/configs/pxe-default.conf $RPM_BUILD_ROOT%{_localstatedir}/lib/tftpboot/ltsp/i386/pxelinux.cfg/default
install -m 0644 server/xinetd.d/nbdrootd $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d/
install -m 0644 server/xinetd.d/nbdswapd $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d/
install -m 0644 server/configs/nbdswapd.conf $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/
install -m 0644 server/configs/ltsp-build-client-ks.cfg $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/

##SKIPPED:
#/etc/network/if-up.d/ltsp-keys
#/usr/sbin/ltsp-update-image
#/usr/share/ltsp/scripts/start-stop-daemon
#/usr/share/ltsp/scripts/popularity-contest-ltsp
#/usr/share/ltsp/scripts/policy-rc.d.ltsp
#/usr/share/ltsp/plugins/*
#/usr/share/bug/ltsp-server/*
#/usr/share/doc/ltsp-server/*
#/usr/share/man/man8/ltsp-update-sshkeys.8.gz
#/usr/share/man/man8/ltsp-update-kernels.8.gz
#/usr/share/man/man8/ltsp-build-client.8.gz
#/usr/share/man/man8/ltsp-update-image.8.gz

# fedora server-side scripts 
#pushd $RPM_BUILD_ROOT
#  tar xzvf %{SOURCE4}
#popd 

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%doc ChangeLog COPYING TODO

%files client
#%doc ChangeLog COPYING TODO
#%doc /usr/share/doc/ltsp-client/examples/lts.conf
#%doc /usr/share/doc/ltsp-client/examples/lts-parameters.txt
%{_mandir}/man1/getltscfg.1.gz
#%config %{_sysconfdir}/init.d/*
%{_bindir}/getltscfg
%{_bindir}/xrexecd
#/sbin/ltsp-client-configure
#/sbin/mkinitrd-ltsp
%{_datadir}/ltsp
#%{_sysconfdir}/rc.d/init.d/*

%files server
%{_localstatedir}/lib/tftpboot/ltsp/i386
%{_localstatedir}/lib/ltsp/swapfiles/
%config(noreplace) %{_localstatedir}/lib/tftpboot/ltsp/i386/pxelinux.cfg/default

%{_sbindir}/ltsp-build-client
%{_sbindir}/ltsp-update-kernels
%{_datadir}/ltsp/scripts
%{_sysconfdir}/X11/Xsession.d/80_ltsp-sound
%{_sbindir}/ldminfod
%{_sbindir}/ltsp-update-sshkeys
%{_sbindir}/nbdrootd
%{_sbindir}/nbdswapd
%{_sysconfdir}/cron.daily/ltsp-swapfile-delete
%{_sysconfdir}/xinetd.d/nbdrootd
%{_sysconfdir}/xinetd.d/nbdswapd
%dir %{_sysconfdir}/ltsp/
%{_sysconfdir}/ltsp/nbdswapd.conf 

#K12 stuff
%config(noreplace) %{_sysconfdir}/ltsp/ltsp-build-client-ks.cfg
#/usr/sbin/ltsp-initialize
#/usr/share/ltsp/chkconfig.d
#/usr/share/ltsp/scripts.d
#%config /usr/share/ltsp/config/ltsp-dhcpd.conf
#%{_sysconfdir}/rc.d/init.d/*
#%config(noreplace) /etc/ltsp/ltsp-dhcpd.conf
#%config(noreplace) /etc/ltsp/ltsp.conf

%changelog

