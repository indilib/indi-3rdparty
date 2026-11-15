Name: indi-starbook-ten
Version:2.1.3.git
Release: %(T=%{_topdir}; mkdir -p $T 2>/dev/null; [ -f $T/.release-stamp-%{name} ] || date -u +%%Y%%m%%d%%H%%M%%S > $T/.release-stamp-%{name} 2>/dev/null; cat $T/.release-stamp-%{name} 2>/dev/null || date -u +%%Y%%m%%d%%H%%M%%S)%{?dist}
Summary: Instrument Neutral Distributed Interface 3rd party drivers

License: LGPLv2
# See COPYRIGHT file for a description of the licenses and files covered

URL: https://indilib.org
Source0: https://github.com/indilib/indi-3rdparty/archive/master.tar.gz

BuildRequires: cmake
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: libnova-devel
BuildRequires: indi-libs
BuildRequires: indi-devel


%description
INDI is a distributed control protocol designed to operate
astronomical instrumentation. INDI is small, flexible, easy to parse,
and scalable. It supports common DCS functions such as remote control,
data acquisition, monitoring, and a lot more. This is a 3rd party driver.


%prep
%autosetup -v -p1 -n indi-3rdparty-master

%build
# This package tries to mix and match PIE and PIC which is wrong and will
# trigger link errors when LTO is enabled.
# Disable LTO
%define _lto_cflags %{nil}

cd indi-starbook-ten
%cmake
%cmake_build

%install
cd indi-starbook-ten
%cmake_install

%files
%license indi-starbook-ten/COPYING indi-starbook-ten/COPYING.LESSER indi-starbook-ten/COPYRIGHT
%doc indi-starbook-ten/AUTHORS indi-starbook-ten/README
%{_bindir}/*
%{_datadir}/indi


%changelog
* Sun Jul 19 2020 Jim Howard <jh.xsnrg+fedora@gmail.com> 1.8.7.git-1
- update to build from git for copr, credit to Sergio Pascual and Christian Dersch for prior work on spec files
