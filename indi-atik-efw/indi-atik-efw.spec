%define __cmake_in_source_build %{_vpath_builddir}
Name: indi-atik-efw
Version:2.1.3.git
Release: %(date -u +%%Y%%m%%d%%H%%M%%S)%{?dist}
Summary: INDI driver for Atik USB electronic filter wheels

License: LGPLv2
URL: https://indilib.org
Source0: https://github.com/indilib/indi-3rdparty/archive/master.tar.gz

BuildRequires: cmake
BuildRequires: indi-libs
BuildRequires: indi-devel
BuildRequires: libnova-devel
BuildRequires: pkgconfig(libusb-1.0)

%description
INDI driver for Atik USB electronic filter wheels.


%prep
%autosetup -v -p1 -n indi-3rdparty-master

%build
%define _lto_cflags %{nil}

cd indi-atik-efw
%cmake -DINDI_DATA_DIR=/usr/share/indi .
make VERBOSE=1 %{?_smp_mflags} -j4

%install
cd indi-atik-efw
make DESTDIR=%{buildroot} install

%files
%license LICENSE
%{_bindir}/indi_atik_efw
%{_datadir}/indi/indi_atik_efw.xml

%changelog
* Sun May 31 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com> 1.0-1
- Initial package
