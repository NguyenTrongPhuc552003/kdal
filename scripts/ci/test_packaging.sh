#!/bin/sh
# scripts/ci/test_packaging.sh
# Local packaging smoke test - mirrors CI build-deb and build-rpm jobs exactly.
#
# Runs inside Docker containers to replicate the GitHub Actions environment
# before pushing a tag, so packaging failures are caught locally.
#
# Requirements: Docker (running)
#
# Usage:
#   ./scripts/ci/test_packaging.sh           Test both DEB and RPM
#   ./scripts/ci/test_packaging.sh deb       Test DEB only
#   ./scripts/ci/test_packaging.sh rpm       Test RPM only

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

if [ -t 1 ]; then
    BLUE='\033[1;34m' GREEN='\033[1;32m' RED='\033[1;31m'
    YELLOW='\033[1;33m' RESET='\033[0m'
else
    BLUE='' GREEN='' RED='' YELLOW='' RESET=''
fi

info() { printf "${BLUE}==> %s${RESET}\n" "$*"; }
ok()   { printf "${GREEN}  ✓ %s${RESET}\n" "$*"; }
fail() { printf "${RED}  ✗ %s${RESET}\n" "$*" >&2; }
warn() { printf "${YELLOW}  ! %s${RESET}\n" "$*"; }
die()  { fail "$*"; exit 1; }

TARGET="${1:-all}"

command -v docker >/dev/null 2>&1 || die "Docker not found - install Docker and ensure it is running"

docker info >/dev/null 2>&1 || die "Docker daemon is not running"

# ── DEB build (mirrors build-deb CI job exactly) ──────────────────────────────

test_deb() {
    info "Testing DEB packaging (ubuntu:latest, matches CI build-deb job)"

    # NOTE: Use explicit if/return - NOT relying on set -e propagation.
    # POSIX set -e is suppressed inside functions called with "|| FAILED=1",
    # so docker run failure would be silently swallowed without this guard.
    if ! docker run --rm \
        -v "$REPO_ROOT:/workspace:ro" \
        -w /tmp/kdal-deb \
        ubuntu:latest \
        sh -c '
            set -e

            # Mirror CI: Install build dependencies
            apt-get update -qq
            apt-get install -y -qq build-essential debhelper cmake gcc

            # Mirror CI: Copy source tree into a writable directory
            cp -r /workspace/. /tmp/kdal-deb/

            # Mirror CI: Prepare Debian source tree
            cp -r packaging/debian .

            # Mirror CI: Validate source format (mirrors CI validation step)
            FORMAT="$(cat debian/source/format)"
            if [ "$FORMAT" != "3.0 (quilt)" ]; then
                echo "ERROR: debian/source/format must be [3.0 (quilt)], got: [$FORMAT]" >&2
                exit 1
            fi

            # Mirror CI: Build DEB package with -d (skip dep re-check)
            dpkg-buildpackage -us -uc -b -d

            # Verify outputs
            DEB_COUNT=$(ls /tmp/*.deb 2>/dev/null | wc -l)
            if [ "$DEB_COUNT" -eq 0 ]; then
                echo "ERROR: No .deb files produced" >&2
                exit 1
            fi
            echo ""
            echo "==> DEB packages produced:"
            ls -lh /tmp/*.deb
            echo ""
            for deb in /tmp/*.deb; do
                echo "==> Contents of $(basename "$deb"):"
                dpkg -c "$deb"
                echo ""
            done
        '; then
        fail "DEB packaging: FAILED (docker run exited non-zero)"
        return 1
    fi

    ok "DEB packaging: passed"
}

# ── RPM build (mirrors build-rpm CI job exactly) ──────────────────────────────

test_rpm() {
    info "Testing RPM packaging (fedora:latest, matches CI build-rpm job)"

    if ! docker run --rm \
        -v "$REPO_ROOT:/workspace:ro" \
        -w /tmp/kdal-rpm \
        fedora:latest \
        sh -c '
            set -e

            # Mirror CI: Install build dependencies
            dnf install -y -q rpm-build cmake gcc

            # Mirror CI: Copy source tree
            cp -r /workspace/. /tmp/kdal-rpm/

            # Mirror CI: Build RPM
            VERSION="$(cat VERSION)"
            mkdir -p rpmbuild/SOURCES rpmbuild/SPECS
            tar czf "rpmbuild/SOURCES/kdal-${VERSION}.tar.gz" \
                --transform "s|^|kdal-${VERSION}/|" \
                --exclude=".git" \
                --exclude="build" \
                --exclude="rpmbuild" \
                .
            cp packaging/rpm/kdal.spec rpmbuild/SPECS/
            rpmbuild -bb \
                --define "_topdir $(pwd)/rpmbuild" \
                --define "_version ${VERSION}" \
                rpmbuild/SPECS/kdal.spec

            RPM_COUNT=$(find rpmbuild/RPMS -name "*.rpm" 2>/dev/null | wc -l)
            if [ "$RPM_COUNT" -eq 0 ]; then
                echo "ERROR: No .rpm files produced" >&2
                exit 1
            fi
            echo ""
            echo "==> RPM packages produced:"
            find rpmbuild/RPMS -name "*.rpm" -exec ls -lh {} \;
            echo ""
            echo "==> RPM contents:"
            find rpmbuild/RPMS -name "*.rpm" -exec rpm -qlp {} \;
        '; then
        fail "RPM packaging: FAILED (docker run exited non-zero)"
        return 1
    fi

    ok "RPM packaging: passed"
}

# ── Summary ───────────────────────────────────────────────────────────────────

echo ""
info "KDAL local packaging test (Docker)"
info "Repo: $REPO_ROOT"
echo ""

FAILED=0

case "$TARGET" in
    deb)
        test_deb || FAILED=1
        ;;
    rpm)
        test_rpm || FAILED=1
        ;;
    all)
        test_deb || FAILED=1
        echo ""
        test_rpm || FAILED=1
        ;;
    *)
        die "Unknown target: $TARGET - use 'deb', 'rpm', or 'all'"
        ;;
esac

echo ""
if [ "$FAILED" -eq 0 ]; then
    ok "All packaging tests passed - safe to push tag"
else
    die "Packaging test(s) failed - fix before pushing tag"
fi
