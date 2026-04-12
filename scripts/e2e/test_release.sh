#!/bin/sh
# E2E Test - Release Pipeline Validation
#
# Simulates: A release engineer validating that the release tooling
# is correct - bump_version updates all 8 sources, tarball layout is
# right, workflows exist, and the release gate checks pass.
#
# Usage: ./scripts/e2e/test_release.sh

set -eu

# shellcheck disable=SC2034
E2E_SUITE="release"
. "$(dirname "$0")/lib.sh"

e2e_mktemp
SANDBOX="$E2E_TMPDIR"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 1: Release scripts exist and are valid"
# ══════════════════════════════════════════════════════════════════════

# shellcheck disable=SC2043
for script in scripts/release/bump_version.sh; do
	assert_file_exists "$E2E_ROOT/$script" "$script"
	assert_cmd "$script is valid shell" sh -n "$E2E_ROOT/$script"
done

# Optional release scripts
for script in scripts/release/generate_changelog.sh \
	scripts/release/update_homebrew_sha.sh; do
	if [ -f "$E2E_ROOT/$script" ]; then
		e2e_pass "$script exists"
		assert_cmd "$script is valid shell" sh -n "$E2E_ROOT/$script"
	else
		e2e_skip "$script not found"
	fi
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 2: bump_version dry-run"
# ══════════════════════════════════════════════════════════════════════

# Clone the repo into sandbox to test bump_version non-destructively
e2e_info "Copying project to sandbox for bump_version test..."
cp -r "$E2E_ROOT" "$SANDBOX/kdal-test"

# Save original version
_orig_ver="$(cat "$SANDBOX/kdal-test/VERSION" 2>/dev/null || echo "0.0.0")"
TEST_VER="99.99.99"

if (cd "$SANDBOX/kdal-test" && sh scripts/release/bump_version.sh "$TEST_VER") >/dev/null 2>&1; then
	e2e_pass "bump_version runs successfully"

	# Validate all 8 version sources were updated
	# 1. VERSION
	if grep -q "$TEST_VER" "$SANDBOX/kdal-test/VERSION"; then
		e2e_pass "VERSION updated to $TEST_VER"
	else
		e2e_fail "VERSION not updated"
	fi

	# 2. version.h
	if grep -q "KDAL_VERSION_STRING.*$TEST_VER" \
		"$SANDBOX/kdal-test/include/kdal/core/version.h" 2>/dev/null; then
		e2e_pass "version.h updated"
	else
		e2e_fail "version.h not updated"
	fi

	# 3. Debian changelog
	if grep -q "$TEST_VER" "$SANDBOX/kdal-test/packaging/debian/changelog" 2>/dev/null; then
		e2e_pass "debian/changelog updated"
	else
		e2e_fail "debian/changelog not updated"
	fi

	# 4. RPM spec
	if grep -q "Version:.*$TEST_VER" "$SANDBOX/kdal-test/packaging/rpm/kdal.spec" 2>/dev/null; then
		e2e_pass "kdal.spec updated"
	else
		e2e_fail "kdal.spec not updated"
	fi

	# 5. CITATION.cff
	if grep -q "version: $TEST_VER" "$SANDBOX/kdal-test/CITATION.cff" 2>/dev/null; then
		e2e_pass "CITATION.cff updated"
	else
		e2e_fail "CITATION.cff not updated"
	fi

	# 6. VS Code package.json
	if grep -q "\"version\": \"$TEST_VER\"" \
		"$SANDBOX/kdal-test/editor/vscode/kdal-lang/package.json" 2>/dev/null; then
		e2e_pass "VS Code package.json updated"
	else
		e2e_fail "VS Code package.json not updated"
	fi

	# 7. kdalup.sh
	if grep -q "KDALUP_VERSION=\"$TEST_VER\"" \
		"$SANDBOX/kdal-test/scripts/installer/kdalup.sh" 2>/dev/null; then
		e2e_pass "kdalup.sh updated"
	else
		e2e_fail "kdalup.sh not updated"
	fi

	# 8. Homebrew formula
	if grep -q "v${TEST_VER}" "$SANDBOX/kdal-test/packaging/homebrew/kdal.rb" 2>/dev/null; then
		e2e_pass "Homebrew formula updated"
	else
		e2e_fail "Homebrew formula not updated"
	fi
else
	e2e_fail "bump_version script failed"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 3: GitHub Actions workflows"
# ══════════════════════════════════════════════════════════════════════

WF_DIR="$E2E_ROOT/.github/workflows"

# All workflows should exist
for wf in build.yml ci.yml test.yml codeql.yml release.yml docs.yml nightly.yml; do
	assert_file_exists "$WF_DIR/$wf" "workflows/$wf"
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 4: release.yml content validation"
# ══════════════════════════════════════════════════════════════════════

RELEASE_YML="$WF_DIR/release.yml"

assert_file_contains "$RELEASE_YML" "release" "workflow name"
assert_file_contains "$RELEASE_YML" "matrix" "uses build matrix"
assert_file_contains "$RELEASE_YML" "SHA256\|sha256\|checksum" "generates checksums"
assert_file_contains "$RELEASE_YML" "upload\|artifact" "uploads artifacts"
assert_file_contains "$RELEASE_YML" "linux-x86_64\|x86_64" "targets x86_64"
assert_file_contains "$RELEASE_YML" "aarch64\|arm64" "targets aarch64"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 5: nightly.yml content validation"
# ══════════════════════════════════════════════════════════════════════

NIGHTLY_YML="$WF_DIR/nightly.yml"

assert_file_contains "$NIGHTLY_YML" "workflow_dispatch" "has manual trigger"
assert_file_contains "$NIGHTLY_YML" "test\|build\|lint" "runs tests"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 6: YAML syntax validation"
# ══════════════════════════════════════════════════════════════════════

if require_tool python3; then
	_yaml_ok=true
	for wf in "$WF_DIR"/*.yml; do
		_name="$(basename "$wf")"
		if python3 -c "import yaml; yaml.safe_load(open('$wf'))" 2>/dev/null; then
			e2e_pass "$_name is valid YAML"
		else
			e2e_fail "$_name is invalid YAML"
			_yaml_ok=false
		fi
	done
else
	e2e_skip "python3 not available - skip YAML validation"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 7: Simulated tarball layout"
# ══════════════════════════════════════════════════════════════════════

# Build SDK and verify tarball structure (offline simulation)
if [ -x "$E2E_ROOT/compiler/kdalc" ] && [ -x "$E2E_ROOT/tools/kdality/kdality" ]; then
	TAR_STAGE="$SANDBOX/tarball"
	mkdir -p "$TAR_STAGE/bin" "$TAR_STAGE/include" "$TAR_STAGE/lib" \
		"$TAR_STAGE/share/kdal/stdlib" "$TAR_STAGE/share/kdal/vim"

	# Simulate what the release job creates
	cp "$E2E_ROOT/compiler/kdalc" "$TAR_STAGE/bin/"
	cp "$E2E_ROOT/tools/kdality/kdality" "$TAR_STAGE/bin/"
	cp "$E2E_ROOT/lang/stdlib"/*.kdh "$TAR_STAGE/share/kdal/stdlib/"

	# Package it
	(cd "$SANDBOX" && tar czf "kdal-sdk-test.tar.gz" -C tarball .)

	assert_file_not_empty "$SANDBOX/kdal-sdk-test.tar.gz" "SDK tarball"

	# Verify tarball contents
	_tar_files="$(tar tzf "$SANDBOX/kdal-sdk-test.tar.gz" | wc -l | tr -d ' ')"
	if [ "$_tar_files" -ge 8 ]; then
		e2e_pass "tarball contains $_tar_files entries (>= 8)"
	else
		e2e_fail "tarball only has $_tar_files entries (expected >= 8)"
	fi

	# Verify key files in tarball
	for entry in "./bin/kdalc" "./bin/kdality"; do
		if tar tzf "$SANDBOX/kdal-sdk-test.tar.gz" | grep -q "$(echo "$entry" | sed 's|^\./||')"; then
			e2e_pass "tarball contains $entry"
		else
			e2e_fail "tarball missing $entry"
		fi
	done

	# SHA256 checksum generation
	if command -v sha256sum >/dev/null 2>&1; then
		sha256sum "$SANDBOX/kdal-sdk-test.tar.gz" >"$SANDBOX/SHA256SUMS"
		e2e_pass "SHA256SUMS generated (sha256sum)"
	elif command -v shasum >/dev/null 2>&1; then
		shasum -a 256 "$SANDBOX/kdal-sdk-test.tar.gz" >"$SANDBOX/SHA256SUMS"
		e2e_pass "SHA256SUMS generated (shasum)"
	else
		e2e_skip "no sha256 tool available"
	fi

	if [ -f "$SANDBOX/SHA256SUMS" ]; then
		assert_file_not_empty "$SANDBOX/SHA256SUMS" "SHA256SUMS"
	fi
else
	e2e_skip "binaries not built - skip tarball test"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_summary
