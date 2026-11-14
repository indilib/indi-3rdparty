Name: indi-atik-efw
Version:2.1.3.git
Release: %(T=%{_topdir}; mkdir -p $T 2>/dev/null; [ -f $T/.release-stamp-%{name} ] || date -u +%%Y%%m%%d%%H%%M%%S > $T/.release-stamp-%{name} 2>/dev/null; cat $T/.release-stamp-%{name} 2>/dev/null || date -u +%%Y%%m%%d%%H%%M%%S)%{?dist}
Summary: INDI driver for Atik serial electronic filter wheels

License: LGPLv2
URL: https://indilib.org
Source0: https://github.com/indilib/indi-3rdparty/archive/master.tar.gz

BuildRequires: cmake
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: indi-libs
BuildRequires: indi-devel
BuildRequires: libnova-devel

%description
INDI driver for Atik serial electronic filter wheels.


%prep
%autosetup -v -p1 -n indi-3rdparty-master

%build
%define _lto_cflags %{nil}

cd indi-atik-efw
%cmake -DINDI_DATA_DIR=/usr/share/indi
%cmake_build

%install
cd indi-atik-efw
%cmake_install

%files
%license LICENSE
%{_bindir}/indi_atik_efw
%{_datadir}/indi/indi_atik_efw.xml
/usr/lib/udev/rules.d/99-atik-efw1.rules

%changelog
* Sun May 31 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com> 1.0-1
- Initial package
