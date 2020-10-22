#
# spec file for package ogon
#
#

Name:           ogon-core
Version:        1.0.0
Release:        1
%if %{defined fedora}
License:        ASL 2.0
%else
License:        Apache-2.0
%endif
Summary:        Remote Desktop Services - RDP server and session manager
Url:            http://ogon-project.com/
%if %{defined fedora}
Group:          Applications/Communications
%else
Group:          Productivity/Networking/Other
%endif
BuildRequires:  freerdp2-devel
BuildRequires:  pam-devel
BuildRequires:  cmake
BuildRequires:  boost-devel
BuildRequires:  systemd-devel
BuildRequires:  gcc-c++
BuildRequires:  libXau-devel
%if %{defined fedora}
BuildRequires:  openssl-devel
BuildRequires:  protobuf-compiler
BuildRequires:  protobuf-c-devel
BuildRequires:  thrift-devel
BuildRequires:  dbus-devel
%else
BuildRequires:  libopenssl-devel
BuildRequires:  protobuf-c
BuildRequires:  libprotobuf-c-devel
BuildRequires:  protobuf-devel
BuildRequires:  dbus-1-devel
%if %{defined is_opensuse}
BuildRequires:  libthrift-devel
%else
BuildRequires:  thrift-lib-cpp-devel
%endif
%if 0%{?suse_version} >= 1210
BuildRequires: systemd-rpm-macros
%endif
%{?systemd_requires}
%if %{defined fedora}
BuildRequires: systemd
%endif

%endif
Source0:        ogon-core-%{version}.tar.xz
%if %{defined suse_version}
Source1:	ogon-core-rpmlintrc
%endif
#Source2:        ogon-rdp-server.preset
#Source3:        ogon-session-manager.preset
BuildRoot:      %{_tmppath}/%{name}-%{version}-build


%description
ogon Remote Desktop Services provide graphical remote access to
desktop sessions and applications using the Remote Desktop Protocol
(RDP) and supports most modern RDP protocol extensions, bitmap
compression codecs and device redirections. ogon is build on the
FreeRDP library and is compatible with virtually any existing Remote
Desktop Client.
* H.246, RemoteFX, planar and interleaved bitmapcompression
* Microsoft Remote Desktop Services API compatibility (aka WTSAPI)
* Virtual channels: drive, clipboard, audio, multi-touch and gfx
  pipeline
* Session reconnect, shadowing and messaging
* Automatic bandwith management
* command line and web interfaces for user and session management
* X11 and Qt backends included 

This package contains the ogon Remote Desktop Protocol (RDP)
server and the ogon Session Manager.

%package -n libogon-backend1
Summary: Remote Desktop Services - backend library
%if !%{defined fedora}
Group:          Development/Libraries/C and C++
%else
Group:          Development/Libraries
%endif
%description -n libogon-backend1
ogon Remote Desktop Services provide graphical remote access to
desktop sessions and applications using the Remote Desktop Protocol
(RDP) and supports most modern RDP protocol extensions, bitmap
compression codecs and device redirections. ogon is build on the
FreeRDP library and is compatible with virtually any existing Remote
Desktop Client.
* H.246, RemoteFX, planar and interleaved bitmapcompression
* Microsoft Remote Desktop Services API compatibility (aka WTSAPI)
* Virtual channels: drive, clipboard, audio, multi-touch and gfx
  pipeline
* Session reconnect, shadowing and messaging
* Automatic bandwith management
* command line and web interfaces for user and session management
* X11 and Qt backends included 

This package contains the ogon backend library

%package -n ogon-devel
Summary: Remote Desktop Services - development support files
Group:   Development/Productivity/Networking/Other
Requires: libogon-backend1 = %{version}
Requires: freerdp2-devel
%if !%{defined fedora}
Group:          Development/Libraries/C and C++
%else
Group:          Development/Libraries
%endif
%description -n ogon-devel
ogon Remote Desktop Services provide graphical remote access to
desktop sessions and applications using the Remote Desktop Protocol
(RDP) and supports most modern RDP protocol extensions, bitmap
compression codecs and device redirections. ogon is build on the
FreeRDP library and is compatible with virtually any existing Remote
Desktop Client.
* H.246, RemoteFX, planar and interleaved bitmapcompression
* Microsoft Remote Desktop Services API compatibility (aka WTSAPI)
* Virtual channels: drive, clipboard, audio, multi-touch and gfx
  pipeline
* Session reconnect, shadowing and messaging
* Automatic bandwith management
* command line and web interfaces for user and session management
* X11 and Qt backends included

This package contains the ogon development files.


%prep
%setup -q -n ogon-core-%{version}

%build
%cmake \
  -DCMAKE_BUILD_TYPE=RELWITHDEBINFO \
  -DWITH_DBUS=ON \
  -DWITH_PAM=ON \
  -DCMAKE_SKIP_RPATH=FALSE \
  -DCMAKE_SKIP_INSTALL_RPATH=TRUE \
  -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} \
  -DCMAKE_INSTALL_LIBDIR:PATH=%{_lib} \
  -DSYSCONFDIR=%{_sysconfdir} \
  -DLOCALSTATEDIR=%{_localstatedir} \
  -DWITH_OPENH264=ON \
  -DWITH_LIBSYSTEMD=ON \
  -Wno-dev \
%if %{defined fedora}
    ..
%endif


make %{?_smp_mflags}

%install
%if !0%{?fedora}
#export NO_BRP_STRIP_DEBUG=true
#export NO_DEBUGINFO_STRIP_DEBUG=true
#%define __debug_install_post %{nil}
cd build
%endif
make %{?_smp_mflags} DESTDIR=%{buildroot} install
cd ..
install -d -m755 %{buildroot}/%{_unitdir}
install -m 644 misc/config.ini.debian %{buildroot}%{_sysconfdir}/ogon/config.ini
install -m 644 build/misc/ogon-session-manager.service %{buildroot}%{_unitdir}/ogon-session-manager.service
install -m 644 build/misc/ogon-rdp-server.service %{buildroot}%{_unitdir}/ogon-rdp-server.service
mkdir -p %{buildroot}%{_sysconfdir}/pam.d
%if %{defined fedora}
install -m 644 misc/pam.d/ogon.fedora %{buildroot}%{_sysconfdir}/pam.d/ogon
%else
install -m 644 misc/pam.d/ogon.suse %{buildroot}%{_sysconfdir}/pam.d/ogon
%endif



%if %{defined suse_version}
%if 0%{?suse_version} > 1220
ln -s %{_sbindir}/service %{buildroot}%{_sbindir}/rcogon-session-manager
ln -s %{_sbindir}/service %{buildroot}%{_sbindir}/rcogon-rdp-server
%else
ln -s /sbin/service %{buildroot}%{_sbindir}/ogon-session-manager
ln -s /sbin/service %{buildroot}%{_sbindir}/ogon-rdp-server
%endif
%endif


%files
%defattr(-,root,root)
%{_libdir}/ogon1
%{_sbindir}/ogon-backend-launcher
%{_sbindir}/ogon-get-openh264-codec
%{_sbindir}/ogon-rdp-server
%{_sbindir}/ogon-session-manager
%{_bindir}/ogon-cli
%{_bindir}/ogon-snmon
%config %{_sysconfdir}/dbus-1/system.d/ogon.SessionManager.conf
%attr(0644,root,root) %config(noreplace) %{_sysconfdir}/pam.d/ogon
%dir %{_sysconfdir}/ogon
%{_sysconfdir}/ogon/ogonXsession
%{_sysconfdir}/ogon/ogon_cleanup.sh
%config %{_sysconfdir}/ogon/config.ini
%{_unitdir}/ogon-rdp-server.service
%{_unitdir}/ogon-session-manager.service
%{_sbindir}/rcogon-session-manager
%{_sbindir}/rcogon-rdp-server

%files -n ogon-devel
%defattr(-,root,root)
%{_libdir}/libogon-backend.so
%{_includedir}/ogon1
%{_libdir}/cmake/ogon-backend1
%{_libdir}/cmake/ogon1
%{_libdir}/pkgconfig/ogon-backend1.pc
%{_libdir}/pkgconfig/ogon1.pc
%{_datarootdir}/ogon/1
%dir %{_datarootdir}/ogon

%files -n libogon-backend1
%defattr(-,root,root)
%{_libdir}/libogon-backend.so.1
%{_libdir}/libogon-backend.so.1.0.0

%if %{defined suse_version}
%pre -n ogon-core
%service_add_pre ogon-rdp-server.service ogon-session-manager.service

#%%pre -n ogon-core -p /bin/bash
#%%systemd_preset_pre
#%%posttrans -p /bin/bash
#%%systemd_preset_posttrans

%post -n ogon-core
%service_add_post ogon-rdp-server.service ogon-session-manager.service

%preun -n ogon-core
%service_del_preun ogon-rdp-server.service ogon-session-manager.service

%postun -n ogon-core
%service_del_postun ogon-rdp-server.service ogon-session-manager.service

%else

%post
%systemd_post ogon-rdp-server.service
%systemd_post ogon-session-manager.service

%preun
%systemd_preun ogon-rdp-server.service
%systemd_preun ogon-session-manager.service

%postun
%systemd_postun_with_restart ogon-rdp-server.service
%systemd_postun_with_restart ogon-session-manager.service
%endif

%post -n libogon-backend1 -p /sbin/ldconfig
%postun -n libogon-backend1 -p /sbin/ldconfig

%changelog
