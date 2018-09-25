Name: klog
Summary: the tools for KUX
Version: 1.0.2
Release: 2.kux
Vendor: INSPUR
License: GPL
URL: www.inspur.com
Group: System Environment/Kernel
Source: %{name}-%{version}.tgz
Buildroot: %{_tmppath}/%{name}-%{version}-root
Requires: systemd

%description
the crash message collector for KUX 3.2

%prep
%setup
#%patch

%build
make -C src clean
make -C src

%install
	make -C src INSTALL_PATH=$RPM_BUILD_ROOT install

%clean
	[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%pre
	
%post
	depmod -a
	
%preun
	systemctl stop klog.service >/dev/null 2>&1
	
%postun
	depmod -a
	
%files
%defattr(-,root,root)
/usr/sbin/*
/etc/*
/usr/lib/systemd/system/*
/lib/modules/*

%changelog
