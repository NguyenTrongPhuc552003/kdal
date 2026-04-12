#!/bin/sh
# KDAL development run script.
#
# Purpose: run a target.  If the target has not been built yet,
# automatically calls build.sh to compile it first.
#
# Usage:
#   ./scripts/dev/run.sh [--variant <name>] <target> [args...]
#   ./scripts/dev/run.sh [--preset <name>] <target> [args...]
#
# Executable targets (args forwarded to binary):
#   kdalc     <args>   - run the KDAL compiler
#   kdality   <args>   - run the KDAL CLI toolchain
#
# Service targets (no extra args):
#   website            - start Astro dev server (localhost:4321)
#   vscode             - open VS Code with the extension loaded
#   module             - load the kernel module (requires sudo + Linux)
#
# Check / analysis targets:
#   ci                 - run CI smoke tests
#   e2e    [suite]     - run end-to-end test suites
#   lint               - run checkpatch + static analysis
#
# Variants:
#   release     - optimized local SDK/toolchain build in build/release/
#   debug       - symbols + assertions for local debugging in build/debug/
#   asan        - AddressSanitizer build for memory debugging in build/asan/

set -eu

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT_DIR="${REPO_ROOT}/scripts/dev"
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

if [ -z "${TARGET}" ]; then
	echo "Usage: $0 [--variant <release|debug|asan>] <target> [args...]"
	echo "       $0 [--preset <name>] <target> [args...]"
	echo ""
	echo "Targets:"
	echo "  kdalc   <args>   Run the KDAL compiler"
	echo "  kdality <args>   Run the KDAL CLI toolchain"
	echo "  website          Start Astro dev server"
	echo "  vscode           Open VS Code with KDAL extension"
	echo "  module           Load the kernel module (sudo, Linux)"
	echo "  ci               Run CI smoke tests"
	echo "  e2e    [suite]   Run end-to-end test suites"
	echo "  lint             Run checkpatch + static analysis"
	exit 1
fi

shift

# Helper: ensure target is built before running
ensure_built() {
	"${SCRIPT_DIR}/build.sh" --preset "${DEV_PRESET}" "$@"
}

case "${TARGET}" in

# ── Executable targets ───────────────────────────────────────────
kdalc)
	ensure_built kdalc
	exec "${BUILD_DIR}/compiler/kdalc" "$@"
	;;

kdality)
	ensure_built kdality
	exec "${BUILD_DIR}/kdality" "$@"
	;;

# ── Service targets ──────────────────────────────────────────────
website)
	ensure_built website
	echo "==> Starting Astro dev server..."
	cd "${REPO_ROOT}/website"
	exec npx astro preview "$@"
	;;

vscode)
	ensure_built vscode
	VSIX=$(find "${REPO_ROOT}/editor/vscode/kdal-lang" -maxdepth 1 -name '*.vsix' | head -1)
	if [ -n "${VSIX}" ]; then
		echo "==> Installing KDAL VS Code extension..."
		code --install-extension "${VSIX}" "$@"
	else
		echo "Error: .vsix not found after build"
		exit 1
	fi
	;;

module)
	ensure_built module
	KO=$(find "${REPO_ROOT}" -maxdepth 1 -name 'kdal.ko' | head -1)
	if [ -z "${KO}" ]; then
		echo "Error: kdal.ko not found - module build may have failed."
		exit 1
	fi
	echo "==> Loading kernel module..."
	sudo insmod "${KO}" "$@"
	echo "    Module loaded.  Use 'sudo rmmod kdal' to unload."
	;;

# ── Check / analysis targets ─────────────────────────────────────
ci)
	ensure_built ci
	;;

e2e)
	ensure_built e2e
	;;

lint)
	ensure_built lint
	;;

*)
	echo "Error: unknown target '${TARGET}'"
	echo "Run '$0' without arguments to see available targets."
	exit 1
	;;
esac
