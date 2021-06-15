%define __cmake_in_source_build %{_vpath_builddir}
Name: libahp_xc
Version:1.9.1.git
Release: %(date -u +%%Y%%m%%d%%H%%M%%S)%{?dist}
Summary: Instrument Neutral Distributed Interface 3rd party drivers

License: LGPLv3
# See COPYRIGHT file for a description of the licenses and files covered

URL: https://indilib.org
Source0: https://github.com/indilib/indi-3rdparty/archive/master.tar.gz

%global debug_package %{nil}
%define __find_requires %{nil}


BuildRequires: cmake
BuildRequires: libfli-devel
BuildRequires: libnova-devel
BuildRequires: qt5-qtbase-devel
BuildRequires: systemd
BuildRequires: gphoto2-devel
BuildRequires: LibRaw-devel
BuildRequires: indi-libs
BuildRequires: indi-devel
BuildRequires: libtiff-devel
BuildRequires: cfitsio-devel
BuildRequires: zlib-devel
BuildRequires: gsl-devel
BuildRequires: libcurl-devel
BuildRequires: libjpeg-turbo-devel
BuildRequires: fftw-devel
BuildRequires: libftdi-devel
BuildRequires: gpsd-devel
BuildRequires: libdc1394-devel
BuildRequires: boost-devel
BuildRequires: boost-regex

BuildRequires: gmock

BuildRequires: pkgconfig(fftw3)
BuildRequires: pkgconfig(cfitsio)
BuildRequires: pkgconfig(libcurl)
BuildRequires: pkgconfig(gsl)
BuildRequires: pkgconfig(libjpeg)
BuildRequires: pkgconfig(libusb-1.0)
BuildRequires: pkgconfig(zlib)

Provides: libahp_xc.so()(64bit)
Provides: libahp_xc.so

%description
INDI is a distributed control protocol designed to operate
astronomical instrumentation. INDI is small, flexible, easy to parse,
and scalable. It supports common DCS functions such as remote control,
data acquisition, monitoring, and a lot more. This is a 3rd party driver.


%prep -v
%setup -n %{name}-%{version}

%build
# This package tries to mix and match PIE and PIC which is wrong and will
# trigger link errors when LTO is enabled.
# Disable LTO
%define _lto_cflags %{nil}

cd libahp_xc
%cmake .
make VERBOSE=1 %{?_smp_mflags} -j4

%install
cd libahp_xc
find %buildroot -type f \( -name '*.so' -o -name '*.so.*' \) -exec chmod 755 {} +
make DESTDIR=%{buildroot} install

%files
%{_libdir}/*
%{_includedir}/ahp


%changelog
* Sun Jul 19 2020 Jim Howard <jh.xsnrg+fedora@gmail.com> 1.8.7.git-1
- update to build from git for copr, credit to Sergio Pascual and Christian Dersch for prior work on spec files

