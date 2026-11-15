Name: indi-libcamera
Version:2.1.3.git
Release: %(T=%{_topdir}; mkdir -p $T 2>/dev/null; [ -f $T/.release-stamp-%{name} ] || date -u +%%Y%%m%%d%%H%%M%%S > $T/.release-stamp-%{name} 2>/dev/null; cat $T/.release-stamp-%{name} 2>/dev/null || date -u +%%Y%%m%%d%%H%%M%%S)%{?dist}
Summary: Instrument Neutral Distributed Interface 3rd party drivers

License: LGPLv2
# See COPYRIGHT file for a description of the licenses and files covered

URL: https://indilib.org
Source0: https://github.com/indilib/indi-3rdparty/archive/master.tar.gz

# rpicam-apps (libcamera-apps-devel) exists only on Fedora 44+: on Fedora 43
# x86_64 it is the pre-rpicam_app-soname 1.5.0, and on aarch64 it is not
# packaged at all; RHEL/EPEL has no libcamera either.  Skip cleanly there.
%if (0%{?fedora} && 0%{?fedora} < 44) || (0%{?rhel} && 0%{?rhel} < 9)
%define skip_build 1
ExcludeArch: x86_64 aarch64
%endif

# Rawhide (and once branched, Fedora 46+) ships a libcamera release that
# removed properties::PixelArraySize, which this driver still calls; the
# code needs an upstream fix before it can build there again.
%if (0%{?fedora} && 0%{?fedora} >= 46)
%define skip_build 1
ExcludeArch: x86_64 aarch64
%endif

%if !0%{?skip_build}
BuildRequires: cmake
BuildRequires: gcc
BuildRequires: gcc-c++
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
BuildRequires: libftdi-devel
BuildRequires: libcamera-devel
BuildRequires: libdc1394-devel
BuildRequires: boost-devel
BuildRequires: boost-regex
BuildRequires: libasi
%endif
BuildRequires: ffmpeg-free-devel
BuildRequires: libavcodec-free-devel
BuildRequires: libcamera-apps-devel

BuildRequires: gmock

BuildRequires: pkgconfig(fftw3)
BuildRequires: pkgconfig(cfitsio)
BuildRequires: pkgconfig(libcurl)
BuildRequires: pkgconfig(gsl)
BuildRequires: pkgconfig(libjpeg)
BuildRequires: pkgconfig(libusb-1.0)
BuildRequires: pkgconfig(zlib)

Requires: libasi

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

cd indi-libcamera
# Fedora ships upstream libcamera (no Raspberry Pi controls) on non-RPi arches;
# the rpicam-apps headers gate their RPi-only code behind DISABLE_RPI_FEATURES.
# A cache init file injects the define on both old and new cmake-rpm-macros.
%ifnarch aarch64
mkdir -p build-init
cat > build-init/init-cache.txt <<'CACHE'
set(ENV{CXXFLAGS} "$ENV{CXXFLAGS} -DDISABLE_RPI_FEATURES")
CACHE
%endif
%ifnarch aarch64
%cmake -DINDI_DATA_DIR=/usr/share/indi -C build-init/init-cache.txt
%else
%cmake -DINDI_DATA_DIR=/usr/share/indi
%endif
%cmake_build

%install
cd indi-libcamera
%cmake_install

%files
%{_bindir}/*
%{_datadir}/indi

%changelog
* Sun Jul 19 2020 Jim Howard <jh.xsnrg+fedora@gmail.com> 1.8.7.git-1
- update to build from git for copr, credit to Sergio Pascual and Christian Dersch for prior work on spec files

