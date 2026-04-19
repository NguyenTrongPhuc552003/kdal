#!/bin/sh
# kdalup - KDAL SDK installer and version manager.
#
# Installs the KDAL toolchain (kdalc, kdality, stdlib, headers, CMake
# files) from pre-built GitHub Release tarballs.
#
# Install layout (KDAL_HOME defaults to ~/.kdal):
#   $KDAL_HOME/bin/kdalc          Compiler binary
#   $KDAL_HOME/bin/kdality        Unified CLI tool
#   $KDAL_HOME/bin/kdalup         This script (self-managed)
#   $KDAL_HOME/include/kdal/      Kernel-space headers
#   $KDAL_HOME/include/kdalc/     Compiler headers
#   $KDAL_HOME/lib/libkdalc.a     Compiler static library
#   $KDAL_HOME/share/kdal/stdlib/ Standard library .kdh files
#   $KDAL_HOME/lib/cmake/KDAL/    CMake package files
#
# Usage:
#   kdalup install [version]              Install SDK (default: latest)
#   kdalup install [version] --profile P  Install with profile (full|minimal)
#   kdalup update                         Update to latest version
#   kdalup self-update                    Update kdalup itself
#   kdalup list                           List installed components
#   kdalup uninstall                      Remove SDK completely
#   kdalup doctor                         Verify installation health
#   kdalup version                        Show installed version
#   kdalup help                           Show this help
#
# Profiles:
#   minimal (default)  Compiler (kdalc), CLI (kdality), stdlib
#   full               Everything + kernel headers, CMake package, Vim syntax
#
# Environment:
#   KDAL_HOME     Install prefix (default: ~/.kdal)
#   KDAL_GITHUB   GitHub repo   (default: NguyenTrongPhuc552003/kdal)

set -eu

# ── Constants ─────────────────────────────────────────────────────────

KDALUP_VERSION="0.1.0"
KDAL_HOME="${KDAL_HOME:-$HOME/.kdal}"
KDAL_GITHUB="${KDAL_GITHUB:-NguyenTrongPhuc552003/kdal}"
KDAL_RELEASES="https://github.com/${KDAL_GITHUB}/releases"

# ── Output helpers ────────────────────────────────────────────────────

info() { printf '\033[1;34m==> %s\033[0m\n' "$1"; }
warn() { printf '\033[1;33m==> %s\033[0m\n' "$1"; }
err() { printf '\033[1;31merror: %s\033[0m\n' "$1" >&2; }
ok() { printf '\033[1;32m==> %s\033[0m\n' "$1"; }

# ── Platform detection ────────────────────────────────────────────────

detect_os() {
	case "$(uname -s)" in
	Linux*) echo "linux" ;;
	Darwin*) echo "darwin" ;;
	*)
		err "Unsupported OS: $(uname -s)"
		exit 1
		;;
	esac
}

detect_arch() {
	case "$(uname -m)" in
	x86_64 | amd64) echo "x86_64" ;;
	aarch64 | arm64) echo "aarch64" ;;
	riscv64) echo "riscv64" ;;
	*)
		err "Unsupported architecture: $(uname -m)"
		exit 1
		;;
	esac
}

# ── Downloader ────────────────────────────────────────────────────────

need_cmd() {
	if ! command -v "$1" >/dev/null 2>&1; then
		err "Required command not found: $1"
		exit 1
	fi
}

# Pick curl or wget; prefer curl
downloader() {
	if command -v curl >/dev/null 2>&1; then
		echo "curl"
	elif command -v wget >/dev/null 2>&1; then
		echo "wget"
	else
		err "Neither curl nor wget found. Please install one."
		exit 1
	fi
}

download() {
	_url="$1"
	_output="$2"
	_dl="$(downloader)"

	case "$_dl" in
	curl)
		curl -fsSL --retry 3 --retry-delay 2 -o "$_output" "$_url"
		;;
	wget)
		wget -q --tries=3 -O "$_output" "$_url"
		;;
	esac
}

# ── Version resolution ────────────────────────────────────────────────

# Resolve "latest" to an actual tag via the GitHub Releases API.
# Primary: JSON API endpoint (works even if the redirect returns 404).
# Fallback: HTML redirect trick for environments without JSON parsing.
resolve_latest_version() {
	_api_url="https://api.github.com/repos/${KDAL_GITHUB}/releases/latest"
	_tag=""

	# ── Primary: GitHub Releases JSON API ────────────────────────────
	if command -v curl >/dev/null 2>&1; then
		_tag="$(curl -fsSL --retry 3 --retry-delay 2 \
			-H 'Accept: application/vnd.github+json' \
			"$_api_url" 2>/dev/null |
			grep '"tag_name"' | head -1 |
			sed 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')" || true
	elif command -v wget >/dev/null 2>&1; then
		_tag="$(wget -q -O - \
			--header='Accept: application/vnd.github+json' \
			"$_api_url" 2>/dev/null |
			grep '"tag_name"' | head -1 |
			sed 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')" || true
	fi

	# ── Fallback: redirect URL trick ─────────────────────────────────
	if [ -z "${_tag:-}" ]; then
		if command -v curl >/dev/null 2>&1; then
			_tag="$(curl -fsSL -o /dev/null -w '%{url_effective}' \
				"${KDAL_RELEASES}/latest" 2>/dev/null |
				grep -oE 'v[0-9]+\.[0-9]+\.[0-9]+[a-zA-Z0-9._-]*$')" || true
		elif command -v wget >/dev/null 2>&1; then
			_tag="$(wget -q --max-redirect=0 -O /dev/null \
				"${KDAL_RELEASES}/latest" 2>&1 |
				grep -oE 'v[0-9]+\.[0-9]+\.[0-9]+[a-zA-Z0-9._-]*')" || true
		fi
	fi

	if [ -z "${_tag:-}" ]; then
		err "Could not determine latest version from GitHub."
		err "Try specifying a version: kdalup install v0.1.0"
		exit 1
	fi

	echo "$_tag"
}

# Ensure version has 'v' prefix
normalize_version() {
	case "$1" in
	v*) echo "$1" ;;
	*) echo "v$1" ;;
	esac
}

# ── Tarball naming ────────────────────────────────────────────────────

# Expected tarball: kdal-<version>-<os>-<arch>.tar.gz
tarball_name() {
	_ver="$1"
	_os="$2"
	_arch="$3"
	echo "kdal-${_ver}-${_os}-${_arch}.tar.gz"
}

tarball_url() {
	_ver="$1"
	_tarball="$2"
	echo "${KDAL_RELEASES}/download/${_ver}/${_tarball}"
}

# ── SHA256 verification ──────────────────────────────────────────────

sha256_cmd() {
	if command -v sha256sum >/dev/null 2>&1; then
		echo "sha256sum"
	elif command -v shasum >/dev/null 2>&1; then
		echo "shasum -a 256"
	else
		echo ""
	fi
}

# Download and verify SHA256SUMS for a release artifact.
# Returns 0 on success, 1 on failure, 2 if no checksums available.
verify_sha256() {
	_file="$1"
	_filename="$(basename "$_file")"
	_ver="$2"
	_sha_cmd="$(sha256_cmd)"

	if [ -z "$_sha_cmd" ]; then
		warn "sha256sum/shasum not found - skipping integrity check"
		return 2
	fi

	_sums_url="${KDAL_RELEASES}/download/${_ver}/SHA256SUMS"
	_sums_file="$(mktemp)"

	if ! download "$_sums_url" "$_sums_file" 2>/dev/null; then
		rm -f "$_sums_file"
		warn "SHA256SUMS not available for ${_ver} - skipping integrity check"
		return 2
	fi

	_expected="$(grep "$_filename" "$_sums_file" | head -1 | awk '{print $1}')"
	rm -f "$_sums_file"

	if [ -z "$_expected" ]; then
		warn "No checksum found for ${_filename} - skipping integrity check"
		return 2
	fi

	_actual="$(eval "$_sha_cmd" "$_file" | awk '{print $1}')"

	if [ "$_expected" = "$_actual" ]; then
		info "SHA256 verified ✓"
		return 0
	else
		err "SHA256 mismatch!"
		err "  Expected: ${_expected}"
		err "  Got:      ${_actual}"
		return 1
	fi
}

# ── Shell profile detection ──────────────────────────────────────────

detect_profile() {
	_shell="$(basename "${SHELL:-/bin/sh}")"
	case "$_shell" in
	bash)
		if [ -f "$HOME/.bash_profile" ]; then
			echo "$HOME/.bash_profile"
		elif [ -f "$HOME/.bashrc" ]; then
			echo "$HOME/.bashrc"
		else
			echo "$HOME/.profile"
		fi
		;;
	zsh)
		echo "${ZDOTDIR:-$HOME}/.zshrc"
		;;
	fish)
		echo "$HOME/.config/fish/config.fish"
		;;
	*)
		echo "$HOME/.profile"
		;;
	esac
}

# Add KDAL_HOME/bin to PATH in shell profile.
configure_path() {
	_profile="$(detect_profile)"
	_marker='# Added by kdalup'
	_export_line="export PATH=\"\$HOME/.kdal/bin:\$PATH\" $_marker"

	if [ -f "$_profile" ] && grep -qF "$_marker" "$_profile"; then
		return 0 # Already configured
	fi

	info "Adding KDAL to PATH in $_profile"
	printf '\n# KDAL SDK\n%s\n' "$_export_line" >>"$_profile"
}

# ── Subcommands ───────────────────────────────────────────────────────

cmd_install() {
	_os="$(detect_os)"
	_arch="$(detect_arch)"
	_profile="minimal"

	# Parse arguments: [version] [--profile full|minimal]
	while [ $# -gt 0 ]; do
		case "$1" in
		--profile)
			shift
			case "${1:-}" in
			full | minimal) _profile="$1" ;;
			*)
				err "Invalid profile: ${1:-}. Use 'full' or 'minimal'."
				exit 1
				;;
			esac
			shift
			;;
		-*)
			err "Unknown option: $1"
			exit 1
			;;
		*)
			_explicit_version="$1"
			shift
			;;
		esac
	done

	if [ -n "${_explicit_version:-}" ]; then
		_version="$(normalize_version "$_explicit_version")"
	else
		info "Resolving latest version..."
		_version="$(resolve_latest_version)"
	fi

	info "Installing KDAL SDK ${_version} (${_os}/${_arch}, profile: ${_profile})"

	_tarball="$(tarball_name "$_version" "$_os" "$_arch")"
	_url="$(tarball_url "$_version" "$_tarball")"
	_tmpdir="$(mktemp -d)"
	_archive="${_tmpdir}/${_tarball}"

	# Clean up on exit
	trap 'rm -rf "$_tmpdir"' EXIT

	info "Downloading ${_tarball}..."
	if ! download "$_url" "$_archive"; then
		err "Failed to download: $_url"
		err ""
		err "This could mean:"
		err "  - Version ${_version} does not exist"
		err "  - No pre-built binary for ${_os}/${_arch}"
		err ""
		err "Check available releases: ${KDAL_RELEASES}"
		rm -rf "$_tmpdir"
		exit 1
	fi

	# Verify integrity
	if ! verify_sha256 "$_archive" "$_version"; then
		_ret=$?
		if [ "$_ret" -eq 1 ]; then
			err "Aborting install due to checksum mismatch."
			rm -rf "$_tmpdir"
			exit 1
		fi
		# _ret == 2 means no checksums available, continue
	fi

	info "Extracting to ${KDAL_HOME}..."
	mkdir -p "$KDAL_HOME"

	# Tarball is expected to contain a top-level directory.
	# Strip it and install directly into KDAL_HOME.
	tar xzf "$_archive" -C "$_tmpdir"

	# Find the extracted directory (kdal-v0.1.0-linux-x86_64/ or similar)
	_extracted="$(find "$_tmpdir" -mindepth 1 -maxdepth 1 -type d | head -1)"

	if [ -z "$_extracted" ]; then
		err "Tarball did not contain a directory - unexpected format."
		rm -rf "$_tmpdir"
		exit 1
	fi

	# Profile-based filtering: minimal strips kernel headers, cmake, vim
	if [ "$_profile" = "minimal" ]; then
		rm -rf "${_extracted}/include/kdal"
		rm -rf "${_extracted}/include/kdalc"
		rm -rf "${_extracted}/lib/libkdalc.a"
		rm -rf "${_extracted}/lib/cmake"
		rm -rf "${_extracted}/share/kdal/vim"
	fi

	# Copy SDK contents into KDAL_HOME, merging directories
	cp -R "${_extracted}/"* "$KDAL_HOME/"

	# Install kdalup itself
	if [ -f "$0" ]; then
		mkdir -p "${KDAL_HOME}/bin"
		cp "$0" "${KDAL_HOME}/bin/kdalup"
		chmod +x "${KDAL_HOME}/bin/kdalup"
	fi

	# Record installed version and profile
	echo "$_version" >"${KDAL_HOME}/.installed-version"
	echo "$_profile" >"${KDAL_HOME}/.installed-profile"

	rm -rf "$_tmpdir"
	trap - EXIT

	# Set up PATH
	configure_path

	# Install Vim/Neovim syntax files if bundled in tarball
	if [ -d "${KDAL_HOME}/share/kdal/vim" ]; then
		_vim_src="${KDAL_HOME}/share/kdal/vim"
		for _vimdir in ftdetect syntax ftplugin compiler plugin; do
			if [ -d "${_vim_src}/${_vimdir}" ]; then
				mkdir -p "${HOME}/.vim/${_vimdir}"
				cp -R "${_vim_src}/${_vimdir}/"* "${HOME}/.vim/${_vimdir}/"
			fi
		done
		# Neovim filetype.lua
		if [ -f "${_vim_src}/filetype.lua" ]; then
			mkdir -p "${HOME}/.local/share/nvim/site"
			cp "${_vim_src}/filetype.lua" "${HOME}/.local/share/nvim/site/filetype.lua"
		fi
	fi

	ok "KDAL SDK ${_version} installed to ${KDAL_HOME}"
	echo ""
	echo "  Components:"
	_count=0
	for _bin in kdalc kdality; do
		if [ -f "${KDAL_HOME}/bin/${_bin}" ]; then
			_count=$((_count + 1))
			echo "    $(printf '\033[32m✓\033[0m') ${_bin}"
		fi
	done
	[ -d "${KDAL_HOME}/include/kdal" ] && echo "    $(printf '\033[32m✓\033[0m') kernel headers"
	[ -d "${KDAL_HOME}/include/kdalc" ] && echo "    $(printf '\033[32m✓\033[0m') compiler headers"
	[ -f "${KDAL_HOME}/lib/libkdalc.a" ] && echo "    $(printf '\033[32m✓\033[0m') libkdalc.a"
	[ -d "${KDAL_HOME}/share/kdal/stdlib" ] && echo "    $(printf '\033[32m✓\033[0m') stdlib (.kdh)"
	[ -d "${KDAL_HOME}/lib/cmake/KDAL" ] && echo "    $(printf '\033[32m✓\033[0m') CMake package"
	[ -d "${HOME}/.vim/syntax" ] && [ -f "${HOME}/.vim/syntax/kdh.vim" ] &&
		echo "    $(printf '\033[32m✓\033[0m') Vim syntax files"
	echo ""
	echo "  To get started, restart your shell or run:"
	echo ""
	echo "    export PATH=\"\$HOME/.kdal/bin:\$PATH\""
	echo ""
}

cmd_update() {
	if [ ! -f "${KDAL_HOME}/.installed-version" ]; then
		err "KDAL SDK is not installed. Run: kdalup install"
		exit 1
	fi

	_current="$(cat "${KDAL_HOME}/.installed-version")"
	info "Current version: ${_current}"
	info "Checking for updates..."

	_latest="$(resolve_latest_version)"

	if [ "$_current" = "$_latest" ]; then
		ok "Already up to date (${_current})"
		return 0
	fi

	info "Updating ${_current} → ${_latest}"
	cmd_install "$_latest"
}

cmd_self_update() {
	info "Updating kdalup..."

	_dl="$(downloader)"
	_url="https://raw.githubusercontent.com/${KDAL_GITHUB}/main/scripts/installer/kdalup.sh"
	_tmpfile="$(mktemp)"

	if ! download "$_url" "$_tmpfile"; then
		err "Failed to download latest kdalup"
		rm -f "$_tmpfile"
		exit 1
	fi

	mkdir -p "${KDAL_HOME}/bin"
	mv "$_tmpfile" "${KDAL_HOME}/bin/kdalup"
	chmod +x "${KDAL_HOME}/bin/kdalup"

	ok "kdalup updated successfully"
}

cmd_list() {
	echo "KDAL SDK - ${KDAL_HOME}"
	echo ""

	if [ -f "${KDAL_HOME}/.installed-version" ]; then
		echo "  Version: $(cat "${KDAL_HOME}/.installed-version")"
	else
		echo "  Version: (not installed)"
		return 0
	fi

	echo "  OS/Arch: $(detect_os)/$(detect_arch)"
	echo ""
	echo "  Components:"

	_check() {
		if [ -e "$1" ]; then
			printf '    \033[32m✓\033[0m %s\n' "$2"
		else
			printf '    \033[31m✗\033[0m %s\n' "$2"
		fi
	}

	_check "${KDAL_HOME}/bin/kdalc" "kdalc        - KDAL compiler"
	_check "${KDAL_HOME}/bin/kdality" "kdality      - unified CLI tool"
	_check "${KDAL_HOME}/bin/kdalup" "kdalup       - SDK manager (this tool)"
	_check "${KDAL_HOME}/include/kdal" "kdal headers - kernel-space API"
	_check "${KDAL_HOME}/include/kdalc" "kdalc headers— compiler API"
	_check "${KDAL_HOME}/lib/libkdalc.a" "libkdalc.a   - compiler library"
	_check "${KDAL_HOME}/share/kdal/stdlib" "stdlib       - .kdh standard library"
	_check "${KDAL_HOME}/lib/cmake/KDAL" "CMake        - find_package(KDAL)"
	_check "${HOME}/.vim/syntax/kdh.vim" "Vim syntax   - .kdh/.kdc highlighting"

	if [ -d "${KDAL_HOME}/share/kdal/stdlib" ]; then
		echo ""
		echo "  Standard library modules:"
		for _f in "${KDAL_HOME}/share/kdal/stdlib/"*.kdh; do
			[ -f "$_f" ] && echo "    • $(basename "$_f" .kdh)"
		done
	fi
	echo ""
}

cmd_uninstall() {
	if [ ! -d "$KDAL_HOME" ]; then
		warn "KDAL SDK is not installed at ${KDAL_HOME}"
		return 0
	fi

	printf 'Remove KDAL SDK from %s? [y/N] ' "$KDAL_HOME"
	read -r _confirm
	case "$_confirm" in
	[yY] | [yY][eE][sS]) ;;
	*)
		echo "Cancelled."
		return 0
		;;
	esac

	info "Removing ${KDAL_HOME}..."
	rm -rf "$KDAL_HOME"

	# Remove PATH entry from profile
	_profile="$(detect_profile)"
	_marker='# Added by kdalup'
	if [ -f "$_profile" ] && grep -qF "$_marker" "$_profile"; then
		info "Cleaning PATH from $_profile"
		# Use a temp file for portability (BSD sed -i differs from GNU)
		_tmp="$(mktemp)"
		grep -vF "$_marker" "$_profile" >"$_tmp"
		mv "$_tmp" "$_profile"
	fi

	ok "KDAL SDK uninstalled"
}

cmd_version() {
	echo "kdalup ${KDALUP_VERSION}"
	if [ -f "${KDAL_HOME}/.installed-version" ]; then
		echo "KDAL SDK $(cat "${KDAL_HOME}/.installed-version")"
	else
		echo "KDAL SDK: not installed"
	fi
	if [ -f "${KDAL_HOME}/.installed-profile" ]; then
		echo "Profile: $(cat "${KDAL_HOME}/.installed-profile")"
	fi
}

cmd_doctor() {
	echo "KDAL SDK Health Check"
	echo "====================="
	echo ""

	_ok=0
	_warn=0
	_fail=0

	_check_exists() {
		if [ -e "$1" ]; then
			printf '  \033[32m✓\033[0m %s\n' "$2"
			_ok=$((_ok + 1))
		else
			printf '  \033[31m✗\033[0m %s  (%s)\n' "$2" "$3"
			_fail=$((_fail + 1))
		fi
	}

	_check_exec() {
		if command -v "$1" >/dev/null 2>&1; then
			_ver="$("$1" --version 2>&1 | head -1 || echo "unknown")"
			printf '  \033[32m✓\033[0m %s  (%s)\n' "$2" "$_ver"
			_ok=$((_ok + 1))
		else
			printf '  \033[31m✗\033[0m %s  (not in PATH)\n' "$2"
			_fail=$((_fail + 1))
		fi
	}

	echo "Installation:"
	if [ -f "${KDAL_HOME}/.installed-version" ]; then
		printf '  \033[32m✓\033[0m Version: %s\n' "$(cat "${KDAL_HOME}/.installed-version")"
		_ok=$((_ok + 1))
	else
		printf '  \033[31m✗\033[0m KDAL SDK not installed\n'
		_fail=$((_fail + 1))
	fi

	_profile="unknown"
	[ -f "${KDAL_HOME}/.installed-profile" ] && _profile="$(cat "${KDAL_HOME}/.installed-profile")"
	echo "  Profile: ${_profile}"
	echo ""

	echo "Core binaries:"
	_check_exists "${KDAL_HOME}/bin/kdalc" "kdalc compiler" "missing - reinstall SDK"
	_check_exists "${KDAL_HOME}/bin/kdality" "kdality CLI" "missing - reinstall SDK"
	echo ""

	echo "Libraries:"
	_check_exists "${KDAL_HOME}/share/kdal/stdlib" "Standard library" "missing - reinstall SDK"
	if [ "$_profile" = "full" ]; then
		_check_exists "${KDAL_HOME}/include/kdal" "Kernel headers" "install with --profile full"
		_check_exists "${KDAL_HOME}/lib/cmake/KDAL" "CMake package" "install with --profile full"
	fi
	echo ""

	echo "Environment:"
	# Check PATH includes KDAL_HOME/bin
	case ":${PATH}:" in
	*":${KDAL_HOME}/bin:"*)
		printf '  \033[32m✓\033[0m PATH includes %s/bin\n' "$KDAL_HOME"
		_ok=$((_ok + 1))
		;;
	*)
		printf '  \033[33m!\033[0m PATH does not include %s/bin\n' "$KDAL_HOME"
		echo "      Add this to your shell profile:"
		echo "        export PATH=\"\$HOME/.kdal/bin:\$PATH\""
		_warn=$((_warn + 1))
		;;
	esac

	# Check KDAL_HOME is set
	if [ -n "${KDAL_HOME:-}" ]; then
		printf '  \033[32m✓\033[0m KDAL_HOME=%s\n' "$KDAL_HOME"
		_ok=$((_ok + 1))
	else
		printf '  \033[33m!\033[0m KDAL_HOME not set (using default ~/.kdal)\n'
		_warn=$((_warn + 1))
	fi
	echo ""

	# Summary
	echo "─────────────────────────────"
	printf '  %d passed, %d warnings, %d failed\n' "$_ok" "$_warn" "$_fail"
	if [ "$_fail" -gt 0 ]; then
		echo ""
		echo "  Run 'kdalup install' to fix missing components."
		return 1
	elif [ "$_warn" -gt 0 ]; then
		echo "  Some optional configuration is missing."
		return 0
	else
		echo "  Everything looks good!"
		return 0
	fi
}

cmd_help() {
	cat <<'EOF'
kdalup - KDAL SDK installer and version manager

USAGE:
    kdalup <command> [arguments]

COMMANDS:
    install [version] [--profile P]  Install SDK (default: latest, minimal)
    update                           Update SDK to latest version
    self-update                      Update kdalup itself
    list                             List installed components
    doctor                           Verify installation health
    uninstall                        Remove SDK and clean shell profile
    version                          Show installed version
    help                             Show this help

PROFILES:
    minimal (default)   kdalc, kdality, stdlib - enough for driver development
    full                Everything + kernel headers, CMake package, Vim syntax

ENVIRONMENT:
    KDAL_HOME           Install directory (default: ~/.kdal)
    KDAL_GITHUB         GitHub repository (default: NguyenTrongPhuc552003/kdal)

EXAMPLES:
    kdalup install                       Install latest release (minimal)
    kdalup install --profile full        Install everything
    kdalup install v0.1.0                Install specific version
    kdalup doctor                        Check installation health
    kdalup update                        Check and install updates
    kdalup list                          Show what's installed
    kdalup uninstall                     Remove everything

MORE INFO:
    https://github.com/NguyenTrongPhuc552003/kdal
EOF
}

# ── Main dispatch ─────────────────────────────────────────────────────

main() {
	if [ $# -eq 0 ]; then
		cmd_help
		exit 0
	fi

	_cmd="$1"
	shift

	case "$_cmd" in
	install) cmd_install "$@" ;;
	update) cmd_update "$@" ;;
	self-update) cmd_self_update "$@" ;;
	list) cmd_list "$@" ;;
	doctor) cmd_doctor "$@" ;;
	uninstall) cmd_uninstall "$@" ;;
	version) cmd_version "$@" ;;
	help | --help | -h) cmd_help ;;
	*)
		err "Unknown command: $_cmd"
		echo "Run 'kdalup help' for usage."
		exit 1
		;;
	esac
}

main "$@"
