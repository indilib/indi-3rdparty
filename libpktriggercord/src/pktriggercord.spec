%define name      pktriggercord	
%define ver       0.85.01
%define rel       1
%define prefix    /usr
%define debug_package %{nil}

%ifarch x86_64
%define _arch x86_64
%endif
 
%ifarch i386
%define _arch i386
%endif

%ifarch i686
%define _arch i686
%endif

Summary: Remote control program for Pentax DSLR cameras.
Name: pktriggercord
Version: %ver
Release: %rel
License: LGPL
Group: Applications/Tools
Source: https://github.com/asalamon74/pktriggercord/releases/download/v%{ver}/pkTriggerCord-%{ver}.src.tar.gz
URL: http://pktriggercord.melda.info/
Packager: Andras Salamon <andras.salamon@melda.info>
BuildRoot: /var/tmp/%{name}-root
BuildArch: %{_arch}
BuildRequires: gtk2-devel

%description
pkTriggerCord is a remote control program for Pentax DSLR cameras.

%prep
%setup -q

%build
make clean
make PREFIX=%{prefix}

%install
rm -rf ${RPM_BUILD_ROOT}
make install PREFIX=%{prefix} DESTDIR=${RPM_BUILD_ROOT}

%post
(which setcap && setcap CAP_SYS_RAWIO+eip %{prefix}/bin/pktriggercord-cli) || true
(which setcap && setcap CAP_SYS_RAWIO+eip %{prefix}/bin/pktriggercord) || true

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%doc Changelog COPYING INSTALL BUGS pktriggercord-cli.1 pktriggercord.1
%prefix/bin/*
%prefix/share/pktriggercord/*
%prefix/share/man/*/pktriggercord*
%prefix/../etc/*

%changelog
* Sun Jan 27 2019 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.85.00
* Sat May 13 2017 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.84.04
* Mon Oct 31 2016 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.84.02
* Mon May 09 2016 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.84.00
* Fri Jan 29 2016 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.83.00
* Fri Aug 28 2015 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.82.04
* Sun Jun 28 2015 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.82.02
* Sat Nov 01 2014 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.82.00
* Thu May 01 2014 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.81.04
* Fri Nov 01 2013 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.81.02
* Thu Apr 25 2013 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.81.00
* Thu Apr 04 2013 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.80.00
* Sat Jan 12 2013 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.79.02
* Wed Oct 03 2012 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.79.00
* Sun Jul 29 2012 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.78.00
* Tue Jul 24 2012 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.77.12
* Sun Apr 15 2012 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.77.10
* Sat Mar 24 2012 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.77.08
* Sun Jan 08 2012 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.77.06
* Tue Nov 01 2011 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.77.04
* Sat Sep 03 2011 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.77.02
* Mon Jul 18 2011 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.77.00
* Wed Jul 06 2011 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.76.00
* Thu Jun 30 2011 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.75.02
* Sun Jun 26 2011 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.75.00
* Tue Jun 21 2011 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.74.00
* Sat Jun 11 2011 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.73.00
* Wed May 25 2011 Andras Salamon <andras.salamon@melda.info>
- built from pkTriggerCord 0.72.02
