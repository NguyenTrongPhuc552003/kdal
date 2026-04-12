# KDALPackaging.cmake
# CPack configuration for building KDAL distribution packages.
#
# Produces:
#   kdal-<version>-Linux-x86_64.tar.gz   - generic tarball
#   kdal_<version>_amd64.deb             - Debian/Ubuntu package
#   kdal-<version>-1.x86_64.rpm         - Red Hat/SUSE package (if rpmbuild available)
#
# Usage:
#   cmake --build build --target package
#   cmake --build build --target package_source   (source tarball)
#
# Or manually after cmake:
#   cd build && cpack -G DEB
#   cd build && cpack -G RPM
#   cd build && cpack -G TGZ

cmake_minimum_required(VERSION 3.16)
include(GNUInstallDirs)

# ── Package metadata ──────────────────────────────────────────────────

set(CPACK_PACKAGE_NAME              "kdal")
set(CPACK_PACKAGE_VENDOR            "KDAL Project")
set(CPACK_PACKAGE_CONTACT           "trongphuc.nguyen5520@gmail.com")
set(CPACK_PACKAGE_DESCRIPTION_SHORT "Kernel Device Abstraction Layer")
set(CPACK_PACKAGE_DESCRIPTION
    "KDAL is an open-source Linux kernel framework for writing device drivers "
    "in a high-level declarative language (.kdc/.kdh). It includes the KDAL "
    "runtime library, the kdalc compiler, the kdality unified toolchain, and "
    "a standard library of device headers.")

# Version is read from the PROJECT_VERSION set in the top-level CMakeLists.txt
if(PROJECT_VERSION)
    set(CPACK_PACKAGE_VERSION       "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
else()
    set(CPACK_PACKAGE_VERSION       "0.1.0")
    set(CPACK_PACKAGE_VERSION_MAJOR "0")
    set(CPACK_PACKAGE_VERSION_MINOR "1")
    set(CPACK_PACKAGE_VERSION_PATCH "0")
endif()

set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# ── License and README ───────────────────────────────────────────────

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README  "${CMAKE_SOURCE_DIR}/README.md")

# ── Source package ────────────────────────────────────────────────────

set(CPACK_SOURCE_GENERATOR      "TGZ;ZIP")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-src")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\.git/"
    "/build/"
    "\\.o$"
    "\\.a$"
    "\\.ko$"
    "/\\.obj/"
    "/__pycache__/"
    "\\.pyc$"
)

# ── Binary generators ─────────────────────────────────────────────────

set(CPACK_GENERATOR "TGZ")

# Add DEB if dpkg-deb is available
find_program(_DPKG_DEB dpkg-deb)
if(_DPKG_DEB)
    list(APPEND CPACK_GENERATOR "DEB")
endif()

# Add RPM if rpmbuild is available
find_program(_RPMBUILD rpmbuild)
if(_RPMBUILD)
    list(APPEND CPACK_GENERATOR "RPM")
endif()

# ── DEB-specific settings ─────────────────────────────────────────────

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION    "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY   "optional")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")

# Runtime depends: glibc and the Linux kernel headers are needed
# by drivers compiled from .kdc files, not by kdality itself.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6 (>= 2.17)")

# Dev package would additionally depend on: linux-headers-generic
set(CPACK_DEBIAN_PACKAGE_SUGGESTS
    "linux-headers-generic, gcc-aarch64-linux-gnu")

# ── RPM-specific settings ─────────────────────────────────────────────

set(CPACK_RPM_PACKAGE_GROUP     "Development/Tools")
set(CPACK_RPM_PACKAGE_LICENSE   "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_REQUIRES  "glibc >= 2.17")
set(CPACK_RPM_PACKAGE_SUMMARY   "${CPACK_PACKAGE_DESCRIPTION_SHORT}")
set(CPACK_RPM_PACKAGE_URL       "https://github.com/NguyenTrongPhuc552003/kdal")

# ── Install layout ────────────────────────────────────────────────────
# Follows GNUInstallDirs conventions.
#
# Installed layout:
#   ${CMAKE_INSTALL_BINDIR}/kdality           - unified toolchain binary
#   ${CMAKE_INSTALL_BINDIR}/kdalc             - standalone compiler
#   ${CMAKE_INSTALL_LIBDIR}/libkdalc.a        - compiler library
#   ${CMAKE_INSTALL_INCLUDEDIR}/kdal/         - public headers
#   ${CMAKE_INSTALL_DATADIR}/kdal/stdlib/     - .kdh standard library
#   ${CMAKE_INSTALL_DATADIR}/kdal/vim/        - Vim/Neovim syntax files
#   ${CMAKE_INSTALL_LIBDIR}/cmake/KDAL/       - CMake package files
#   ${CMAKE_INSTALL_MANDIR}/man1/kdality.1    - man page (future)

# NOTE: stdlib and cmake module install rules live in the top-level
# CMakeLists.txt with a curated file list.  Do NOT duplicate them here.

# ── Activate CPack ────────────────────────────────────────────────────

if(NOT CPack_CMake_INCLUDED)
    include(CPack)
endif()
