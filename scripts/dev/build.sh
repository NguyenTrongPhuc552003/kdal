#!/bin/sh
# KDAL development build script.
#
# Purpose: compile the specified target (and nothing more).
# To run targets, use run.sh.  To validate results, use test.sh.
#
# Usage:
#   ./scripts/dev/build.sh [--variant <name>] [target]
#   ./scripts/dev/build.sh [--preset <name>] [target]
#   ./scripts/dev/build.sh all          - build every buildable target
#   ./scripts/dev/build.sh clean        - clean the selected build directory
#   ./scripts/dev/build.sh clean <tgt>  - clean a specific target's artifacts
#   ./scripts/dev/build.sh help         - list available targets
#
# Available targets:
#   kdalc       - KDAL compiler binary
#   kdalc_lib   - KDAL compiler static library (libkdalc.a)
#   kdality     - KDAL CLI toolchain
#   website     - documentation site (Astro Starlight)
#   vscode      - VS Code extension (.vsix)
#   module      - kernel module via kbuild
#   all         - build every buildable target above
#   clean       - remove build artifacts (optionally per-target)
#   install     - install SDK to system (cmake --install)
#
# Variants:
#   release     - optimized local SDK/toolchain build in build/release/
#   debug       - symbols + assertions for local debugging in build/debug/
#   asan        - AddressSanitizer build for memory debugging in build/asan/
#
# Advanced presets:
#   ci-release     - CI-only optimized build in build/ci/release/
#   nightly        - nightly matrix build in build/nightly/<os-arch>/
#   release-matrix - release matrix build in build/release/<os-arch>/
#
# Environment:
#   CMAKE_ARGS  - extra arguments passed to the cmake configure step

set -eu

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "${SCRIPT_DIR}/common.sh"

DEV_PRESET="release"

while [ $# -gt 0 ]; do
	case "$1" in
	--variant)
		if [ $# -lt 2 ]; then
			echo "Error: --variant requires a value (release|debug|asan)"
			exit 1
		fi
		DEV_PRESET="$(dev_preset_from_variant "$2")" || {
			echo "Error: unknown variant '$2'"
			exit 1
		}
		shift 2
		;;
	--preset)
		if [ $# -lt 2 ]; then
			echo "Error: --preset requires a value"
			exit 1
		fi
		if ! dev_is_known_preset "$2"; then
			echo "Error: unknown preset '$2'"
			exit 1
		fi
		DEV_PRESET="$2"
		shift 2
		;;
	--)
		shift
		break
		;;
	*)
		break
		;;
	esac
done

BUILD_DIR="$(dev_build_dir_for_preset "${REPO_ROOT}" "${DEV_PRESET}")" || {
	echo "Error: preset '${DEV_PRESET}' requires KDAL_TARGET_TRIPLET to be set"
	exit 1
}
TARGET="${1:-}"

# ── Helper: ensure cmake is configured ───────────────────────────────
ensure_configured() {
	if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
		echo "==> Configuring preset '${DEV_PRESET}' in ${BUILD_DIR}..."
		cmake --preset "${DEV_PRESET}" ${CMAKE_ARGS:-}
	fi
}

# ── Clean ────────────────────────────────────────────────────────────
if [ "${TARGET}" = "clean" ]; then
	CLEAN_TARGET="${2:-all}"
	if [ "${CLEAN_TARGET}" = "all" ]; then
		echo "==> Cleaning entire build directory..."
		rm -rf "${BUILD_DIR}"
		echo "    Done."
	elif [ "${CLEAN_TARGET}" = "website" ]; then
		echo "==> Cleaning website build artifacts..."
		rm -rf "${REPO_ROOT}/website/dist" "${REPO_ROOT}/website/.astro"
		echo "    Done."
	elif [ "${CLEAN_TARGET}" = "vscode" ]; then
		echo "==> Cleaning VS Code extension artifacts..."
		rm -rf "${REPO_ROOT}/editor/vscode/kdal-lang/out" \
			"${REPO_ROOT}/editor/vscode/kdal-lang/"*.vsix
		echo "    Done."
	elif [ "${CLEAN_TARGET}" = "module" ]; then
		echo "==> Cleaning kernel module artifacts..."
		make -C "${REPO_ROOT}" clean 2>/dev/null || true
		echo "    Done."
	else
		# For compiled targets (kdalc, kdalc_lib, kdality), rebuild-clean via cmake
		ensure_configured
		echo "==> Cleaning target: ${CLEAN_TARGET}..."
		cmake --build "${BUILD_DIR}" --target clean
		echo "    Done (full cmake clean - cmake does not support per-target clean)."
	fi
	exit 0
fi

# ── Post-build: collect outputs into selected build subtree ───────────
collect_outputs() {
	_target="${1:-}"
	case "${_target}" in
	website)
		if [ -d "${REPO_ROOT}/website/dist" ]; then
			mkdir -p "${BUILD_DIR}/website"
			cp -R "${REPO_ROOT}/website/dist/." "${BUILD_DIR}/website/"
		fi
		;;
	vscode)
		_vsix="$(find "${REPO_ROOT}/editor/vscode/kdal-lang" \
			-maxdepth 1 -name '*.vsix' 2>/dev/null | head -1)"
		if [ -n "${_vsix}" ]; then
			mkdir -p "${BUILD_DIR}/editor/vscode"
			cp "${_vsix}" "${BUILD_DIR}/editor/vscode/"
		fi
		;;
	esac
}

# ── Build ────────────────────────────────────────────────────────────
ensure_configured

if [ -z "${TARGET}" ]; then
	echo "==> Building default targets..."
	cmake --build "${BUILD_DIR}"
elif [ "${TARGET}" = "help" ]; then
	echo "Available CMake targets:"
	echo ""
	echo "Presets and goals:"
	echo "  release       Optimized local build in build/release/"
	echo "  debug         Symbols + assertions in build/debug/"
	echo "  asan          AddressSanitizer build in build/asan/"
	echo ""
	echo "  kdalc        KDAL compiler binary"
	echo "  kdalc_lib    KDAL compiler static library"
	echo "  kdality      KDAL CLI toolchain"
	echo "  website      Documentation site (Astro Starlight)"
	echo "  vscode       VS Code extension (.vsix)"
	echo "  module       Kernel module via kbuild"
	echo "  all          Build every target above"
	echo "  clean        Clean build artifacts (clean all | clean <target>)"
	echo "  install      Install SDK (cmake --install ${BUILD_DIR})"
	echo ""
	echo "To run targets:     ./scripts/dev/run.sh --variant <name> <target>"
	echo "To test targets:    ./scripts/dev/test.sh --variant <name> <target>"
	echo ""
	echo "Usage: $0 [--variant <release|debug|asan>] [target]"
	echo "       $0 [--preset <name>] [target]"
elif [ "${TARGET}" = "all" ]; then
	echo "==> Building all KDAL targets..."
	cmake --build --preset "${DEV_PRESET}" --target kdal-all
	collect_outputs website
	collect_outputs vscode
elif [ "${TARGET}" = "install" ]; then
	echo "==> Installing KDAL SDK..."
	cmake --build --preset "${DEV_PRESET}"
	cmake --install "${BUILD_DIR}"
else
	echo "==> Building target: ${TARGET}..."
	if [ -z "${TARGET}" ]; then
		cmake --build --preset "${DEV_PRESET}"
	else
		cmake --build --preset "${DEV_PRESET}" --target "${TARGET}"
	fi
	collect_outputs "${TARGET}"
fi
