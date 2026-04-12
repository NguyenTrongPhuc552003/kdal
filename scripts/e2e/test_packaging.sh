#!/bin/sh
# E2E Test - Packaging Validation (DEB + RPM + Homebrew)
#
# Simulates: A package maintainer verifying that all packaging
# metadata is correct and ready for distribution. Does NOT require
# dpkg-buildpackage or rpmbuild - validates metadata and structure.
#
# Usage: ./scripts/e2e/test_packaging.sh

set -eu

# shellcheck disable=SC2034
E2E_SUITE="packaging"
. "$(dirname "$0")/lib.sh"

PKG_DIR="$E2E_ROOT/packaging"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Debian Package - File Structure"
# ══════════════════════════════════════════════════════════════════════

DEB_DIR="$PKG_DIR/debian"

assert_file_exists "$DEB_DIR/control" "debian/control"
assert_file_exists "$DEB_DIR/rules" "debian/rules"
assert_file_exists "$DEB_DIR/changelog" "debian/changelog"
assert_file_exists "$DEB_DIR/copyright" "debian/copyright"
assert_file_exists "$DEB_DIR/source/format" "debian/source/format"
assert_file_exists "$DEB_DIR/kdal.install" "debian/kdal.install"
assert_file_exists "$DEB_DIR/kdal-dev.install" "debian/kdal-dev.install"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Debian Package - control file"
# ══════════════════════════════════════════════════════════════════════

assert_file_contains "$DEB_DIR/control" "Source: kdal" "source package name"
assert_file_contains "$DEB_DIR/control" "Package: kdal" "binary package name"
assert_file_contains "$DEB_DIR/control" "Package: kdal-dev" "dev package name"
assert_file_contains "$DEB_DIR/control" "Maintainer:" "has maintainer"
assert_file_contains "$DEB_DIR/control" "Build-Depends:" "has build-depends"
assert_file_contains "$DEB_DIR/control" "cmake" "depends on cmake"
assert_file_contains "$DEB_DIR/control" "Homepage:" "has homepage"
assert_file_contains "$DEB_DIR/control" "Description:" "has description"
assert_file_contains "$DEB_DIR/control" "Architecture: any" "architecture: any"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Debian Package - rules file"
# ══════════════════════════════════════════════════════════════════════

assert_file_contains "$DEB_DIR/rules" "#!/usr/bin/make -f" "rules has shebang"
assert_file_contains "$DEB_DIR/rules" "dh " "rules uses dh"
assert_file_contains "$DEB_DIR/rules" "cmake\|CMAKE" "rules uses cmake"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Debian Package - changelog"
# ══════════════════════════════════════════════════════════════════════

assert_file_contains "$DEB_DIR/changelog" "kdal (" "changelog has package name"
assert_file_contains "$DEB_DIR/changelog" "unstable" "changelog targets unstable"
assert_file_contains "$DEB_DIR/changelog" "Initial release" "changelog has entry"

# Version in changelog should match VERSION file
_ver="$(cat "$E2E_ROOT/VERSION" 2>/dev/null || echo "unknown")"
if grep -q "$_ver" "$DEB_DIR/changelog" 2>/dev/null; then
	e2e_pass "changelog version matches VERSION ($_ver)"
else
	e2e_fail "changelog version does not match VERSION file ($_ver)"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Debian Package - copyright (DEP-5)"
# ══════════════════════════════════════════════════════════════════════

assert_file_contains "$DEB_DIR/copyright" "Format:" "copyright has Format field"
assert_file_contains "$DEB_DIR/copyright" "GPL-3.0" "copyright references GPL-3.0"
assert_file_contains "$DEB_DIR/copyright" "Upstream-Name: KDAL" "upstream name"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Debian Package - install files"
# ══════════════════════════════════════════════════════════════════════

assert_file_contains "$DEB_DIR/kdal.install" "kdalc" "kdal.install includes kdalc"
assert_file_contains "$DEB_DIR/kdal.install" "kdality" "kdal.install includes kdality"
assert_file_contains "$DEB_DIR/kdal.install" "libkdalc" "kdal.install includes libkdalc"
assert_file_contains "$DEB_DIR/kdal.install" "stdlib" "kdal.install includes stdlib"
assert_file_contains "$DEB_DIR/kdal-dev.install" "include" "kdal-dev.install includes headers"

# ══════════════════════════════════════════════════════════════════════
e2e_section "RPM Package - Spec File"
# ══════════════════════════════════════════════════════════════════════

RPM_SPEC="$PKG_DIR/rpm/kdal.spec"

assert_file_exists "$RPM_SPEC" "kdal.spec"
assert_file_contains "$RPM_SPEC" "Name:.*kdal" "spec has Name"
assert_file_contains "$RPM_SPEC" "Version:" "spec has Version"
assert_file_contains "$RPM_SPEC" "License:.*GPL" "spec has License"
assert_file_contains "$RPM_SPEC" "URL:" "spec has URL"
assert_file_contains "$RPM_SPEC" "BuildRequires:.*cmake" "spec requires cmake"
assert_file_contains "$RPM_SPEC" "%description" "spec has description"
assert_file_contains "$RPM_SPEC" "%files" "spec has files section"
assert_file_contains "$RPM_SPEC" "%changelog" "spec has changelog"
assert_file_contains "$RPM_SPEC" "kdalc" "spec installs kdalc"
assert_file_contains "$RPM_SPEC" "kdality" "spec installs kdality"
assert_file_contains "$RPM_SPEC" "%package.*devel" "spec has -devel subpackage"

# Version in spec should match VERSION file
if grep -q "Version:.*$_ver" "$RPM_SPEC" 2>/dev/null; then
	e2e_pass "RPM spec version matches VERSION ($_ver)"
else
	e2e_fail "RPM spec version does not match VERSION ($_ver)"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "RPM Package - Spec quality checks"
# ══════════════════════════════════════════════════════════════════════

# Verify essential RPM macros
for macro in "%prep" "%build" "%install" "%autosetup" "%cmake"; do
	assert_file_contains "$RPM_SPEC" "$macro" "spec uses $macro"
done

# Check license file is included
assert_file_contains "$RPM_SPEC" "%license LICENSE" "spec includes LICENSE"
assert_file_contains "$RPM_SPEC" "%doc" "spec includes docs"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Homebrew Formula"
# ══════════════════════════════════════════════════════════════════════

BREW_FORMULA="$PKG_DIR/homebrew/kdal.rb"

assert_file_exists "$BREW_FORMULA" "kdal.rb"
assert_file_contains "$BREW_FORMULA" "class Kdal < Formula" "formula class"
assert_file_contains "$BREW_FORMULA" 'desc "' "formula has description"
assert_file_contains "$BREW_FORMULA" "homepage" "formula has homepage"
assert_file_contains "$BREW_FORMULA" "url " "formula has source URL"
assert_file_contains "$BREW_FORMULA" "license" "formula has license"
assert_file_contains "$BREW_FORMULA" 'depends_on "cmake"' "formula depends on cmake"
assert_file_contains "$BREW_FORMULA" "def install" "formula has install block"
assert_file_contains "$BREW_FORMULA" "def test\|test do" "formula has test block"

# Verify the URL references the right GitHub repo
assert_file_contains "$BREW_FORMULA" "NguyenTrongPhuc552003/kdal" "formula URL matches repo"

# Check for head support
assert_file_contains "$BREW_FORMULA" "head " "formula supports head builds"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Cross-package version consistency"
# ══════════════════════════════════════════════════════════════════════

_ver="$(cat "$E2E_ROOT/VERSION" 2>/dev/null || echo "")"
if [ -z "$_ver" ]; then
	e2e_fail "VERSION file is missing or empty"
else
	e2e_pass "VERSION file contains: $_ver"

	# Check all version sources
	_ver_h="$E2E_ROOT/include/kdal/core/version.h"
	if [ -f "$_ver_h" ] && grep -q "KDAL_VERSION_STRING.*$_ver" "$_ver_h" 2>/dev/null; then
		e2e_pass "version.h matches VERSION"
	else
		e2e_fail "version.h version mismatch"
	fi

	# VS Code extension
	_vscode_pkg="$E2E_ROOT/editor/vscode/kdal-lang/package.json"
	if [ -f "$_vscode_pkg" ] && grep -q "\"version\": \"$_ver\"" "$_vscode_pkg" 2>/dev/null; then
		e2e_pass "VS Code package.json matches VERSION"
	else
		e2e_fail "VS Code package.json version mismatch"
	fi

	# kdalup.sh
	_kdalup="$E2E_ROOT/scripts/installer/kdalup.sh"
	if [ -f "$_kdalup" ] && grep -q "KDALUP_VERSION=\"$_ver\"" "$_kdalup" 2>/dev/null; then
		e2e_pass "kdalup.sh matches VERSION"
	else
		e2e_fail "kdalup.sh version mismatch"
	fi

	# CITATION.cff
	_cite="$E2E_ROOT/CITATION.cff"
	if [ -f "$_cite" ] && grep -q "version: $_ver" "$_cite" 2>/dev/null; then
		e2e_pass "CITATION.cff matches VERSION"
	else
		e2e_fail "CITATION.cff version mismatch"
	fi
fi

# ══════════════════════════════════════════════════════════════════════
e2e_summary
