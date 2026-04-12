#!/bin/sh
# KDAL SDK bootstrap installer.
#
# This is the one-liner entry point:
#   curl -fsSL https://raw.githubusercontent.com/NguyenTrongPhuc552003/kdal/main/scripts/installer/install.sh | sh
#
# What it does:
#   1. Downloads kdalup.sh from the repository
#   2. Installs it to ~/.kdal/bin/kdalup
#   3. Runs `kdalup install` to download the SDK
#
# Environment:
#   KDAL_HOME     Install directory (default: ~/.kdal)
#   KDAL_VERSION  Version to install (default: latest)

set -eu

KDAL_HOME="${KDAL_HOME:-$HOME/.kdal}"
KDAL_GITHUB="${KDAL_GITHUB:-NguyenTrongPhuc552003/kdal}"
KDALUP_URL="https://raw.githubusercontent.com/${KDAL_GITHUB}/main/scripts/installer/kdalup.sh"

info() { printf '\033[1;34m==> %s\033[0m\n' "$1"; }
err() { printf '\033[1;31merror: %s\033[0m\n' "$1" >&2; }
ok() { printf '\033[1;32m==> %s\033[0m\n' "$1"; }

# ── Downloader ────────────────────────────────────────────────────────

download() {
	_url="$1"
	_output="$2"
	if command -v curl >/dev/null 2>&1; then
		curl -fsSL --retry 3 -o "$_output" "$_url"
	elif command -v wget >/dev/null 2>&1; then
		wget -q --tries=3 -O "$_output" "$_url"
	else
		err "Neither curl nor wget found. Please install one."
		exit 1
	fi
}

# ── Main ──────────────────────────────────────────────────────────────

main() {
	echo ""
	echo "  ╔═══════════════════════════════════════╗"
	echo "  ║     KDAL SDK Installer                ║"
	echo "  ║     Kernel Device Abstraction Layer   ║"
	echo "  ╚═══════════════════════════════════════╝"
	echo ""

	info "Downloading kdalup..."

	mkdir -p "${KDAL_HOME}/bin"
	_kdalup="${KDAL_HOME}/bin/kdalup"

	if ! download "$KDALUP_URL" "$_kdalup"; then
		err "Failed to download kdalup from:"
		err "  $KDALUP_URL"
		exit 1
	fi

	chmod +x "$_kdalup"
	ok "kdalup installed to ${_kdalup}"
	echo ""

	# Run install with optional version
	if [ -n "${KDAL_VERSION:-}" ]; then
		"$_kdalup" install "$KDAL_VERSION"
	else
		"$_kdalup" install
	fi
}

main "$@"
