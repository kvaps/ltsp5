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
%ifarch %{ix86} x86_64
# Need pxelinux.0 from syslinux if server is x86
BuildRequires: syslinux
%endif
BuildRequires: tftp-server

# Need location of tftpboot directory from tftp-server
%define _tftpdir %(cat /etc/xinetd.d/tftp |grep server_args | awk -F"-s " {'print $2'} || echo -n "/BOGUS/DIRECTORY")

%description
LTSP client and server

%package client
Summary:        LTSP client
Group:          User Interface/Desktops
Requires:       chkconfig

%description client
LTSP client

%package server
Summary:        LTSP server
Group:          User Interface/Desktops
# needed to install client chroots
Requires:       livecd-tools >= 015
Requires:       tftp-server
Requires:       ltspfs
Requires:       dhcp
Requires:       gettext

%description server
LTSP server

%ifarch %{ix86} x86_64
%package vmclient
Summary:        LTSP Virtual Machine Client
Group:          Applications/Emulators
Requires:       kvm
Requires:       bridge-utils

%description vmclient
Run a qemu-kvm virtual machine as a PXE client.  This allows you to test a 
LTSP server without the hassle of having extra hardware.  Requires 
your system to support hardware virtualization or it will be very slow.
%endif


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
mkdir -p $RPM_BUILD_ROOT%{_sbindir}
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man1/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/rwtab.d/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/X11/
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ltsp/

# server
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man8
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/kickstart/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/mkinitrd/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/init.d/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/
mkdir -p $RPM_BUILD_ROOT%{_sbindir}
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ltsp/scripts/
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ltsp/scripts.d/
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ltsp/chkconfig.d/
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ltsp/plugins/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/cron.daily/
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/lib/ltsp/swapfiles/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/network-scripts/
mkdir -p $RPM_BUILD_ROOT/opt/ltsp

%ifarch %{ix86} x86_64
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
install -m 0644 client/ltsp_config $RPM_BUILD_ROOT/%{_datadir}/ltsp/
install -m 0755 client/screen_session $RPM_BUILD_ROOT/%{_datadir}/ltsp/
install -m 0755 client/configure-x.sh $RPM_BUILD_ROOT/%{_datadir}/ltsp/
install -m 0644 client/initscripts/ltsp-init-common $RPM_BUILD_ROOT/%{_datadir}/ltsp/
install -m 0755 client/initscripts/RPM/ltsp-client-launch $RPM_BUILD_ROOT%{_sbindir}
install -m 0644 client/rwtab.d/k12linux.rwtab $RPM_BUILD_ROOT%{_sysconfdir}/rwtab.d
cp -av client/screen.d $RPM_BUILD_ROOT/%{_datadir}/ltsp/
touch $RPM_BUILD_ROOT%{_sysconfdir}/lts.conf
touch $RPM_BUILD_ROOT%{_localstatedir}/lib/random-seed

### server install
install -m 0755 server/nbdrootd $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/nbdswapd $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ldminfod $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ltsp-update-sshkeys $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ltsp-build-client $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ltsp-update-kernels $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/scripts/k12linux/chroot-creator $RPM_BUILD_ROOT%{_sbindir}
install -m 0755 server/ltsp-swapfile-delete $RPM_BUILD_ROOT%{_sysconfdir}/cron.daily/
install -m 0644 server/xinetd.d/nbdrootd $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d/
install -m 0644 server/xinetd.d/nbdswapd $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d/
install -m 0644 server/xinetd.d/ldminfod $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d/
install -m 0644 server/configs/nbdswapd.conf $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/
cp -pr server/configs/kickstart/* $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/kickstart/
install -m 0644 server/configs/k12linux/mkinitrd/ifcfg-eth0 $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/mkinitrd/
install -m 0644 server/configs/k12linux/mkinitrd/sysconfig-mkinitrd $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/mkinitrd/
install -m 0644 server/configs/k12linux/mkinitrd/sysconfig-network $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/mkinitrd/
cp -pr server/plugins/* $RPM_BUILD_ROOT%{_datadir}/ltsp/plugins/
install -m 0755 server/services/ltsp-dhcpd.init $RPM_BUILD_ROOT%{_sysconfdir}/init.d/ltsp-dhcpd
install -m 0755 server/scripts/k12linux/ltsp-prepare-kernel $RPM_BUILD_ROOT/%{_sbindir}/
install -m 0755 server/scripts/k12linux/ltsp-server-initialize $RPM_BUILD_ROOT/%{_sbindir}/
install -m 0755 server/scripts/k12linux/hosts-update $RPM_BUILD_ROOT/%{_datadir}/ltsp/scripts/
install -m 0755 server/scripts/k12linux/dhcpd-update $RPM_BUILD_ROOT/%{_datadir}/ltsp/scripts/
cp -p server/scripts/k12linux/scripts.d/*   $RPM_BUILD_ROOT%{_datadir}/ltsp/scripts.d/
cp -p server/scripts/k12linux/chkconfig.d/* $RPM_BUILD_ROOT%{_datadir}/ltsp/chkconfig.d/

# configs
install -m 0644 server/configs/k12linux/ifcfg-ltspbr0 $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/network-scripts/
install -m 0644 server/configs/k12linux/ltsp-dhcpd.conf $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/ltsp-dhcpd.conf
install -m 0644 server/configs/k12linux/ltsp-update-kernels.conf $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/
install -m 0644 server/configs/k12linux/ltsp-build-client.conf $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/
install -m 0644 server/configs/k12linux/ltsp-server.conf $RPM_BUILD_ROOT%{_sysconfdir}/ltsp/

for arch in i386 x86_64 ppc ppc64; do
    install -m 0644 server/configs/k12linux/lts.conf $RPM_BUILD_ROOT%{_tftpdir}/ltsp/$arch/
done

echo "K12_DIST=%{dist}" | sed 's/\.//' > $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/k12_dist
cat server/configs/k12linux/k12_dist.template >> $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/k12_dist

%ifarch %{ix86} x86_64
# PXE
install -m 0644 server/configs/pxe-default.conf $RPM_BUILD_ROOT%{_tftpdir}/ltsp/i386/pxelinux.cfg/default
install -m 0644 server/configs/pxe-default.conf $RPM_BUILD_ROOT%{_tftpdir}/ltsp/x86_64/pxelinux.cfg/default
install -m 0644 /usr/lib/syslinux/pxelinux.0 $RPM_BUILD_ROOT%{_tftpdir}/ltsp/i386
install -m 0644 /usr/lib/syslinux/pxelinux.0 $RPM_BUILD_ROOT%{_tftpdir}/ltsp/x86_64
%endif

%ifarch %{ix86} x86_64
# vmclient
install -m 0755 vmclient/ltsp-vmclient           $RPM_BUILD_ROOT%{_sbindir}/
install -m 0755 vmclient/ltsp-qemu-bridge-ifup   $RPM_BUILD_ROOT%{_sbindir}/
%endif

%clean
rm -rf $RPM_BUILD_ROOT


%files client
%defattr(-,root,root,-)
%{_mandir}/man1/getltscfg.1.gz
%{_bindir}/getltscfg
%{_bindir}/xrexecd
%{_sbindir}/ltsp-client-launch
%dir %{_datadir}/ltsp
%{_datadir}/ltsp/configure-x.sh
%{_datadir}/ltsp/ltsp-init-common
%{_datadir}/ltsp/ltsp_config
%{_datadir}/ltsp/screen_session
%{_datadir}/ltsp/screen.d/

# readonly-root related files
%{_sysconfdir}/rwtab.d/*
%{_localstatedir}/lib/random-seed
%config(noreplace) %{_sysconfdir}/lts.conf


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
%ifarch %{ix86} x86_64
%{_tftpdir}/ltsp/i386/pxelinux.0
%{_tftpdir}/ltsp/x86_64/pxelinux.0
%config(noreplace) %{_tftpdir}/ltsp/i386/pxelinux.cfg/default
%config(noreplace) %{_tftpdir}/ltsp/x86_64/pxelinux.cfg/default
%endif

%dir /opt/ltsp

%{_sbindir}/ltsp-build-client
%{_sbindir}/ltsp-prepare-kernel
%{_sbindir}/ltsp-server-initialize
%{_sbindir}/ltsp-update-kernels
%{_datadir}/ltsp/scripts/
%{_datadir}/ltsp/scripts.d/
%{_datadir}/ltsp/chkconfig.d/
%{_datadir}/ltsp/plugins/
%{_sbindir}/ldminfod
%{_sbindir}/ltsp-update-sshkeys
%{_sbindir}/nbdrootd
%{_sbindir}/nbdswapd
%{_sbindir}/chroot-creator
%{_sysconfdir}/cron.daily/ltsp-swapfile-delete
%{_sysconfdir}/init.d/ltsp-dhcpd
%{_sysconfdir}/sysconfig/k12_dist
%config(noreplace) %{_sysconfdir}/xinetd.d/nbdrootd
%config(noreplace) %{_sysconfdir}/xinetd.d/nbdswapd
%config(noreplace) %{_sysconfdir}/xinetd.d/ldminfod
%dir %{_sysconfdir}/ltsp/
# Configuration Files
%config(noreplace)%{_sysconfdir}/sysconfig/network-scripts/ifcfg-ltspbr0
%config(noreplace) %{_sysconfdir}/ltsp/nbdswapd.conf 
%config(noreplace) %{_sysconfdir}/ltsp/ltsp-build-client.conf
%config(noreplace) %{_sysconfdir}/ltsp/ltsp-server.conf
%config(noreplace) %{_sysconfdir}/ltsp/ltsp-dhcpd.conf
%config(noreplace) %{_sysconfdir}/ltsp/ltsp-update-kernels.conf
%dir %{_sysconfdir}/ltsp/kickstart/
%dir %{_sysconfdir}/ltsp/kickstart/Fedora/
%config(noreplace) %{_sysconfdir}/ltsp/kickstart/Fedora/common.ks
%dir %{_sysconfdir}/ltsp/kickstart/Fedora/8/
%dir %{_sysconfdir}/ltsp/kickstart/Fedora/9/
%config(noreplace) %{_sysconfdir}/ltsp/kickstart/*/*/*.ks
%dir %{_sysconfdir}/ltsp/mkinitrd/
%config(noreplace) %{_sysconfdir}/ltsp/mkinitrd/*
%config(noreplace)%{_tftpdir}/ltsp/*/lts.conf


%ifarch %{ix86} x86_64
%files vmclient
%{_sbindir}/ltsp-vmclient
%{_sbindir}/ltsp-qemu-bridge-ifup
%endif

%changelog

