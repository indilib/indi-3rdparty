%global forgeurl https://github.com/xsnrg/indi-3rdparty/

Name: indi-3rdparty
Version: 1.8.6.git
Release: 1%{?dist}
Summary: Instrument Neutral Distributed Interface 3rd party drivers

License: LGPLv2+167
# See COPYRIGHT file for a description of the licenses and files covered

%forgemeta -i

URL: http://www.indilib.org
Source0: %{forgesource}

BuildRequires: cmake
BuildRequires: libfli-devel
BuildRequires: libnova-devel
BuildRequires: qt5-qtbase-devel
BuildRequires: systemd
BuildRequires: indi-libs
BuildRequires: indi-devel

BuildRequires: pkgconfig(fftw3)
BuildRequires: pkgconfig(cfitsio)
BuildRequires: pkgconfig(libcurl)
BuildRequires: pkgconfig(gsl)
BuildRequires: pkgconfig(libjpeg)
BuildRequires: pkgconfig(libusb-1.0)
BuildRequires: pkgconfig(zlib)

Requires: %{name}-libs%{?_isa} = %{version}-%{release}

%description
INDI is a distributed control protocol designed to operate
astronomical instrumentation. INDI is small, flexible, easy to parse,
and scalable. It supports common DCS functions such as remote control,
data acquisition, monitoring, and a lot more. This is a 3rd party driver.


%package devel
Summary: Libraries, includes, etc. used to develop an application with %{name}
Requires: %{name}-libs%{?_isa} = %{version}-%{release}
Requires: %{name}-static%{?_isa} = %{version}-%{release}

%description devel
These are the header files needed to develop a %{name} application

%package libs
Summary: INDI shared libraries

%description libs
These are the shared libraries of INDI.

%package static
Summary: Static libraries, includes, etc. used to develop an application with %{name}
Requires: %{name}-libs%{?_isa} = %{version}-%{release}

%description static
Static library needed to develop a %{name} application

%prep
%forgesetup

%build
# This package tries to mix and match PIE and PIC which is wrong and will
# trigger link errors when LTO is enabled.
# Disable LTO
%define _lto_cflags %{nil}

%cmake .
make VERBOSE=1 %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%ldconfig_scriptlets libs

%files
%license COPYING.BSD COPYING.GPL COPYING.LGPL COPYRIGHT LICENSE
%doc AUTHORS ChangeLog NEWS README
%{_bindir}/*
%{_datadir}/indi
%{_udevrulesdir}/*.rules

%files libs
%license COPYING.BSD COPYING.GPL COPYING.LGPL COPYRIGHT LICENSE
%{_libdir}/*.so.*
%{_libdir}/indi/MathPlugins

%files devel
%{_includedir}/*
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc

%files static
%{_libdir}/*.a

%changelog
* Sun Jul 19 2020 Jim Howard <jh.xsnrg+fedora@gmail.com> 1.8.6.git-1
- update to build from git for copr, credit to Sergio Pascual and Christian Dersch for prior work on spec files

