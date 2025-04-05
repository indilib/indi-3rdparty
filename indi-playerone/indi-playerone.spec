%define __cmake_in_source_build %{_vpath_builddir}
Name: indi-playerone
Version:2.1.0.git
Release: %(date -u +%%Y%%m%%d%%H%%M%%S)%{?dist}
Summary: Instrument Neutral Distributed Interface 3rd party drivers

License: LGPLv2
# See COPYRIGHT file for a description of the licenses and files covered

URL: https://indilib.org
Source0: https://github.com/indilib/indi-3rdparty/archive/master.tar.gz

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
BuildRequires: libplayerone

BuildRequires: gmock

BuildRequires: pkgconfig(fftw3)
BuildRequires: pkgconfig(cfitsio)
BuildRequires: pkgconfig(libcurl)
BuildRequires: pkgconfig(gsl)
BuildRequires: pkgconfig(libjpeg)
BuildRequires: pkgconfig(libusb-1.0)
BuildRequires: pkgconfig(zlib)

Requires: libplayerone

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

cd indi-playerone
%cmake -DINDI_DATA_DIR=/usr/share/indi .
make VERBOSE=1 %{?_smp_mflags} -j4

%install
cd indi-playerone
make DESTDIR=%{buildroot} install

%files
%{_bindir}/*
%{_datadir}/indi

%changelog
* Sat Apr 5 2025 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.8.1
- Update PlayerOnePW SDK to v1.2.2
* Fri Feb 21 2025 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.8.0
* Sat Dec 3 2024 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.7.1
- Update PlayerOnePW SDK to v1.2.1
* Sat Sep 21 2024 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.7.0
* Sun Jul 14 2024 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.6.3
* Sun May 12 2024 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.6.2
* Thu Jan 4 2024 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.6.1
* Wed Sep 20 2023 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.6.0
* Sat Sep 02 2023 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.5.0
* Sat Jul 29 2023 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.4.1
* Thu Jul 20 2023 Jarno Paananen <jarno.paananen@gmail.com>
- Add sensor mode control for cameras that support it
* Tue Jun 27 2023 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.4.0
* Tue May 09 2023 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.3.0
* Sat May 06 2023 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOnePW SDK to v1.2.0
* Thu Apr 27 2023 Hiroshi Saito <hiro3110g@gmail.com>
- BugFix: top quarter noise on bin2 image
* Wed Mar 15 2023 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.2.2
* Thu Mar 2 2023 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.2.1
* Fri Feb 24 2023 Hiroshi Saito <hiro3110g@gmail.com>
- Add single camera driver for multi threading
- Add flip property with bayer compensation
- Add nickname property for multi camera driver
- Update PlayerOneCamera SDK to v3.2.0
* Sun Feb 05 2023 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOnePW SDK to v1.1.0
* Wed Jan 18 2023 Hiroshi Saito <hiro3110g@gmail.com>
- Add PlayerOne Filter Wheel driver
- Update PlayerOneCamera SDK to v3.1.1
* Thu Dec 15 2022 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.1.0
* Tue Sep 13 2022 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.0.4
* Sat Jul 02 2022 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.0.3
* Thu Jun 16 2022 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v3.0.2
* Tue Mar 30 2022 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v2.0.6
* Tue Dec 07 2021 Hiroshi Saito <hiro3110g@gmail.com>
- Update PlayerOneCamera SDK to v2.0.5
* Sat Aug 21 2021 Hiroshi Saito <hiro3110g@gmail.com>
- Create

