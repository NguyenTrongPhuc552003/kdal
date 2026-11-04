#!/bin/sh
# Update the Homebrew formula's URL and SHA256 after a new release.
#
# Downloads the source tarball and computes the checksum, then patches
# packaging/homebrew/kdal.rb in place.
#
# Usage: ./scripts/release/update_homebrew_sha.sh <version>
# Example: ./scripts/release/update_homebrew_sha.sh 0.2.0

set -eu

if [ $# -lt 1 ]; then
	echo "Usage: $0 <version>" >&2
	echo "Example: $0 0.2.0" >&2
	exit 1
fi

VERSION="$1"
GITHUB_REPO="NguyenTrongPhuc552003/kdal"
FORMULA="packaging/homebrew/kdal.rb"

info() { printf '\033[1;34m==> %s\033[0m\n' "$1"; }

if [ ! -f "$FORMULA" ]; then
	echo "Error: $FORMULA not found. Run from the repository root." >&2
	exit 1
fi

# Build the expected source tarball URL
TARBALL_URL="https://github.com/${GITHUB_REPO}/archive/refs/tags/v${VERSION}.tar.gz"

info "Downloading source tarball for v${VERSION}..."
TMPFILE="$(mktemp)"
trap 'rm -f "$TMPFILE"' EXIT

if command -v curl >/dev/null 2>&1; then
	curl -fsSL --retry 3 -o "$TMPFILE" "$TARBALL_URL"
elif command -v wget >/dev/null 2>&1; then
	wget -q -O "$TMPFILE" "$TARBALL_URL"
else
	echo "Error: neither curl nor wget found." >&2
	exit 1
fi

info "Computing SHA256..."
if command -v sha256sum >/dev/null 2>&1; then
	SHA256="$(sha256sum "$TMPFILE" | cut -d' ' -f1)"
elif command -v shasum >/dev/null 2>&1; then
	SHA256="$(shasum -a 256 "$TMPFILE" | cut -d' ' -f1)"
else
	echo "Error: neither sha256sum nor shasum found." >&2
	exit 1
fi

info "SHA256: ${SHA256}"

# Update URL line
info "Updating ${FORMULA}..."
sed -i.bak \
	-e "s|url \"https://github.com/${GITHUB_REPO}/archive/refs/tags/v[^\"]*\.tar\.gz\"|url \"${TARBALL_URL}\"|" \
	-e "s|sha256 \"[a-f0-9]\{64\}\"|sha256 \"${SHA256}\"|" \
	"$FORMULA"
rm -f "${FORMULA}.bak"

info "Done. Review changes with: git diff ${FORMULA}"
