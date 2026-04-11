#!/bin/sh
# Synchronize version numbers across all KDAL release files.
#
# Updates version.h, packaging files, CITATION.cff, and CHANGELOG.
#
# Usage: ./scripts/release/bumpversion.sh <major.minor.patch>
# Example: ./scripts/release/bumpversion.sh 0.2.0

set -eu

if [ $# -lt 1 ]; then
    echo "Usage: $0 <major.minor.patch>" >&2
    exit 1
fi

NEW_VERSION="$1"
MAJOR=$(echo "$NEW_VERSION" | cut -d. -f1)
MINOR=$(echo "$NEW_VERSION" | cut -d. -f2)
PATCH=$(echo "$NEW_VERSION" | cut -d. -f3)

info() { printf "\033[1;34m==> %s\033[0m\n" "$1"; }

# Validate
if [ -z "$MAJOR" ] || [ -z "$MINOR" ] || [ -z "$PATCH" ]; then
    echo "Error: version must be in major.minor.patch format" >&2
    exit 1
fi

info "Bumping KDAL version to $NEW_VERSION"

# 1. include/kdal/core/version.h
info "Updating version.h..."
sed -i.bak \
    -e "s/#define KDAL_VERSION_MAJOR .*/#define KDAL_VERSION_MAJOR $MAJOR/" \
    -e "s/#define KDAL_VERSION_MINOR .*/#define KDAL_VERSION_MINOR $MINOR/" \
    -e "s/#define KDAL_VERSION_PATCH .*/#define KDAL_VERSION_PATCH $PATCH/" \
    -e "s/#define KDAL_VERSION_STRING .*/#define KDAL_VERSION_STRING \"$NEW_VERSION\"/" \
    include/kdal/core/version.h
rm -f include/kdal/core/version.h.bak

# 2. packaging/debian/changelog
info "Updating debian/changelog..."
sed -i.bak "1s/kdal ([^)]*)/kdal ($NEW_VERSION-1)/" packaging/debian/changelog
rm -f packaging/debian/changelog.bak

# 3. packaging/rpm/kdal.spec
info "Updating rpm spec..."
sed -i.bak "s/^Version:.*/Version: $NEW_VERSION/" packaging/rpm/kdal.spec
rm -f packaging/rpm/kdal.spec.bak

# 4. CITATION.cff
if [ -f CITATION.cff ]; then
    info "Updating CITATION.cff..."
    sed -i.bak "s/^version:.*/version: $NEW_VERSION/" CITATION.cff
    rm -f CITATION.cff.bak
fi

info "Version bumped to $NEW_VERSION"
info "Review changes with: git diff"
