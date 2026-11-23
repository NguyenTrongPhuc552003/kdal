Name:           kdal
Version: 0.2.0
Release:        1%{?dist}
Summary:        Kernel Device Abstraction Layer - compiler and toolchain
License:        GPL-3.0-or-later
URL:            https://github.com/NguyenTrongPhuc552003/kdal
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc
BuildRequires:  make

%description
KDAL is an open-source Linux kernel framework for writing portable device
drivers and accelerator backends in a high-level declarative language
(.kdc/.kdh). This package provides the kdalc compiler, the kdality unified
CLI tool, the standard library of device headers, and CMake integration.

%package        devel
Summary:        Development headers for KDAL
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    devel
Development headers for building against the KDAL compiler library
(libkdalc) and the kernel-space API.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md CHANGELOG.md
%{_bindir}/kdalc
%{_bindir}/kdality
%{_libdir}/libkdalc.a
%{_datadir}/kdal/stdlib/*.kdh
%{_datadir}/kdal/vim/
%{_libdir}/cmake/KDAL/*.cmake

%files devel
%{_includedir}/kdal/
%{_includedir}/kdalc/

%changelog
* Tue Nov 24 2026 Trong Phuc Nguyen <trongphuc.ng552003@gmail.com> - 0.2.0-1
- Devices report their resource demand (debugfs kdal/demand, JSON)
- KUnit suites can be built into kdal.ko (KDAL_KUNIT=y)
- Builds and loads against Linux v6.18; registry and init fixes
- kdality: init, lint and simulate fixes; paths validated
- The language, compiler and editor plugins are frozen

* Sat Apr 12 2026 Trong Phuc Nguyen <trongphuc.ng552003@gmail.com> - 0.1.0-1
- Initial release
- kdalc compiler with lexer, parser, semantic analysis, C codegen
- kdality CLI: compile, init, simulate, lint, test-gen, dt-gen
- Standard library: uart, spi, i2c, gpio, gpu, npu device headers
- CMake build system with find_package(KDAL) support
