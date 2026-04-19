#!/bin/sh
# scripts/release/release.sh
# KDAL release automation.
#
# Orchestrates the full local release pipeline:
#   prepare → notes → bump → verify → ship
#
# GitHub Actions then builds and publishes all release artifacts automatically
# when the annotated tag is pushed.
#
# Usage:
#   release.sh [--dry-run] <version>            Full release pipeline
#   release.sh [--dry-run] prepare <version>    Validate prerequisites only
#   release.sh [--dry-run] notes   <version>    Generate & insert changelog section
#   release.sh [--dry-run] bump    <version>    Synchronize version across all files
#   release.sh [--dry-run] verify  [version]    Run local quality checks
#   release.sh [--dry-run] ship    <version>    Commit + annotated tag + push
#   release.sh             post-release <ver>   Update Homebrew formula after CI publishes
#
# Environment:
#   KDAL_REMOTE       Git remote to push to (default: origin)
#   KDAL_BRANCH       Branch that must be checked out (default: main)
#   KDAL_EDIT_NOTES   Set to 1 to open CHANGELOG.md in $EDITOR after note generation
#   EDITOR            Editor for interactive review (default: vi)

set -eu

# ── Paths ──────────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TEMPLATE_DIR="$SCRIPT_DIR/templates"

# ── Configuration ──────────────────────────────────────────────────────────────
KDAL_REMOTE="${KDAL_REMOTE:-origin}"
KDAL_BRANCH="${KDAL_BRANCH:-main}"
KDAL_EDIT_NOTES="${KDAL_EDIT_NOTES:-0}"
DRY_RUN=0

# ── Terminal colors ────────────────────────────────────────────────────────────
if [ -t 1 ]; then
    BLUE='\033[1;34m' GREEN='\033[1;32m' YELLOW='\033[1;33m'
    RED='\033[1;31m' MAGENTA='\033[0;35m' RESET='\033[0m'
else
    BLUE='' GREEN='' YELLOW='' RED='' MAGENTA='' RESET=''
fi

info()   { printf "${BLUE}==> %s${RESET}\n" "$*"; }
ok()     { printf "${GREEN}  ✓ %s${RESET}\n" "$*"; }
warn()   { printf "${YELLOW}  ! %s${RESET}\n" "$*"; }
die()    { printf "${RED}Error: %s${RESET}\n" "$*" >&2; exit 1; }
drymsg() { printf "${MAGENTA}[dry-run] %s${RESET}\n" "$*"; }

# Execute a command, or print it in dry-run mode.
xrun() {
    if [ "$DRY_RUN" -eq 1 ]; then
        drymsg "$*"
    else
        "$@"
    fi
}

# ── Usage ──────────────────────────────────────────────────────────────────────
usage() {
    _ver=$(cat "$REPO_ROOT/VERSION" 2>/dev/null || printf '?')
    cat <<EOF
KDAL Release Tool  (current: $_ver)

Usage:
  $(basename "$0") [--dry-run] <version>            Full release pipeline
  $(basename "$0") [--dry-run] prepare <version>    Validate prerequisites only
  $(basename "$0") [--dry-run] notes   <version>    Generate changelog & insert into CHANGELOG.md
  $(basename "$0") [--dry-run] bump    <version>    Synchronize version across all files
  $(basename "$0") [--dry-run] verify  [version]    Run local quality checks
  $(basename "$0") [--dry-run] ship    <version>    Commit + annotated tag + push
  $(basename "$0")             post-release <ver>   Update Homebrew formula after CI publishes

Options:
  --dry-run    Print what would happen without making any git or file mutations
  -h, --help   Show this message

Environment:
  KDAL_REMOTE       Remote to push to (default: origin)
  KDAL_BRANCH       Branch that must be checked out (default: main)
  KDAL_EDIT_NOTES   Set to 1 to open CHANGELOG.md in \$EDITOR after notes generation
  EDITOR            Editor for interactive review

Examples:
  # Preview the full first release without touching anything:
  $(basename "$0") --dry-run 0.1.0

  # Execute the full release:
  $(basename "$0") 0.1.0

  # After CI has published artifacts, update Homebrew:
  $(basename "$0") post-release 0.1.0

  # Incremental release:
  $(basename "$0") 0.2.0
EOF
}

# ── Helpers ────────────────────────────────────────────────────────────────────
validate_semver() {
    printf '%s' "$1" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$' \
        || die "Invalid version '$1' — expected X.Y.Z (e.g. 0.1.0)"
}

current_version() {
    tr -d '[:space:]' < "$REPO_ROOT/VERSION"
}

# Returns the most recent git tag, or empty string if none exists.
previous_tag() {
    git -C "$REPO_ROOT" describe --tags --abbrev=0 2>/dev/null || printf ''
}

changelog_has_version() {
    grep -q "^## \[$1\]" "$REPO_ROOT/CHANGELOG.md" 2>/dev/null
}

# ── Phase 1: prepare ───────────────────────────────────────────────────────────
cmd_prepare() {
    info "prepare: validating prerequisites for v$VERSION"
    cd "$REPO_ROOT"

    validate_semver "$VERSION"
    ok "Semver format: $VERSION"

    # Branch check
    branch=$(git symbolic-ref --short HEAD 2>/dev/null || printf 'HEAD')
    [ "$branch" = "$KDAL_BRANCH" ] \
        || die "Must release from branch '$KDAL_BRANCH'; currently on '$branch'"
    ok "Branch: $branch"

    # Clean working tree
    [ -z "$(git status --porcelain)" ] \
        || die "Working tree is dirty — commit or stash all changes before releasing"
    ok "Working tree: clean"

    # Tag must not already exist locally or remotely
    if git -C "$REPO_ROOT" rev-parse "refs/tags/v$VERSION" >/dev/null 2>&1; then
        die "Tag v$VERSION already exists locally — use a different version or delete the tag first"
    fi
    ok "Tag v$VERSION: available"

    # Required tools
    for tool in git sed grep date; do
        command -v "$tool" >/dev/null 2>&1 || die "Required tool not found: $tool"
    done
    ok "Required tools: present"

    # Report current version
    cur=$(current_version)
    if [ "$cur" = "$VERSION" ]; then
        ok "VERSION: $cur (already at target — bump will be a no-op)"
    else
        warn "VERSION is $cur — 'bump' will update all files to $VERSION"
    fi

    info "prepare: all checks passed for v$VERSION"
}

# ── Phase 2: notes ─────────────────────────────────────────────────────────────
cmd_notes() {
    info "notes: updating CHANGELOG.md for v$VERSION"
    cd "$REPO_ROOT"

    # If the target version section already exists, preserve it and skip generation.
    if changelog_has_version "$VERSION"; then
        ok "CHANGELOG.md already has a section for v$VERSION — preserving existing content"
        return
    fi

    prev=$(previous_tag)
    today=$(date +%Y-%m-%d)

    # Generate body from git log
    if [ -n "$prev" ]; then
        info "Generating notes from commits since $prev ..."
        raw=$("$SCRIPT_DIR/generate_changelog.sh" "$prev")
    else
        info "No previous tag found — generating notes from full commit history ..."
        raw=$("$SCRIPT_DIR/generate_changelog.sh")
    fi

    # Extract only the body: strip "# Changelog" header line and the "## [...] - DATE" line.
    body=$(printf '%s\n' "$raw" \
        | grep -v '^# Changelog' \
        | grep -v '^## \[')

    new_section="## [$VERSION] - $today
$body"

    if [ "$DRY_RUN" -eq 1 ]; then
        drymsg "Would insert the following section into CHANGELOG.md:"
        printf '%s\n' "$new_section"
        return
    fi

    # Insert new section before the first existing ## [...] section.
    tmpfile=$(mktemp /tmp/kdal-changelog.XXXXXX)
    trap 'rm -f "$tmpfile"' EXIT INT TERM

    awk -v sec="$new_section" '
        /^## \[/ && !done { print sec; print ""; done=1 }
        { print }
    ' "$REPO_ROOT/CHANGELOG.md" > "$tmpfile"

    mv "$tmpfile" "$REPO_ROOT/CHANGELOG.md"
    trap - EXIT INT TERM

    ok "Inserted v$VERSION section into CHANGELOG.md"

    # Optional interactive review
    if [ "$KDAL_EDIT_NOTES" = "1" ]; then
        "${EDITOR:-vi}" "$REPO_ROOT/CHANGELOG.md"
    fi
}

# ── Phase 3: bump ──────────────────────────────────────────────────────────────
cmd_bump() {
    info "bump: synchronizing all version files to $VERSION"
    cd "$REPO_ROOT"

    if [ "$DRY_RUN" -eq 1 ]; then
        drymsg "Would run: $SCRIPT_DIR/bump_version.sh $VERSION"
        return
    fi

    "$SCRIPT_DIR/bump_version.sh" "$VERSION"
    ok "All version files synchronized to $VERSION"
}

# ── Phase 4: verify ────────────────────────────────────────────────────────────
cmd_verify() {
    info "verify: running local quality checks"
    cd "$REPO_ROOT"

    # Shell script linting
    if command -v shellcheck >/dev/null 2>&1; then
        find scripts -name '*.sh' -exec shellcheck {} + 2>/dev/null \
            && ok "shellcheck: all scripts clean" \
            || warn "shellcheck: issues found — review before shipping"
    else
        warn "shellcheck not installed — shell script lint skipped"
    fi

    # Version consistency between VERSION file and version.h
    if [ -f "include/kdal/core/version.h" ]; then
        ver_file=$(current_version)
        ver_h=$(grep 'KDAL_VERSION_STRING' include/kdal/core/version.h \
            | sed 's/.*"\([^"]*\)".*/\1/' | head -1)
        if [ "$ver_file" = "$ver_h" ]; then
            ok "Version consistent: VERSION=$ver_file matches version.h"
        else
            die "Version mismatch: VERSION=$ver_file but version.h says $ver_h — run 'bump' first"
        fi
    fi

    # CHANGELOG must have a section for the target version (when version is known)
    if [ -n "${VERSION:-}" ]; then
        changelog_has_version "$VERSION" \
            || die "CHANGELOG.md has no section for v$VERSION — run 'notes' first"
        ok "CHANGELOG.md: section for v$VERSION present"
    fi

    # Local packaging smoke test (requires Docker)
    if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
        info "Docker available — running local packaging tests (DEB + RPM)"
        if [ "$DRY_RUN" -eq 1 ]; then
            drymsg "Would run: $REPO_ROOT/scripts/ci/test_packaging.sh all"
        else
            "$REPO_ROOT/scripts/ci/test_packaging.sh" all \
                && ok "Packaging smoke tests: passed" \
                || die "Packaging smoke tests failed — fix before shipping"
        fi
    else
        warn "Docker not running — skipping local DEB/RPM packaging tests"
        warn "Run 'scripts/ci/test_packaging.sh' manually before pushing the tag"
    fi

    ok "verify: all local checks passed"
}

# ── Phase 5: ship ──────────────────────────────────────────────────────────────
cmd_ship() {
    info "ship: creating release commit, annotated tag, and pushing v$VERSION"
    cd "$REPO_ROOT"

    today=$(date +%Y-%m-%d)
    repo_url="https://github.com/NguyenTrongPhuc552003/kdal"

    # Build commit message from template or inline default
    commit_file=$(mktemp /tmp/kdal-commit.XXXXXX)
    if [ -f "$TEMPLATE_DIR/release-commit.txt" ]; then
        sed \
            -e "s/{{VERSION}}/$VERSION/g" \
            -e "s/{{DATE}}/$today/g" \
            -e "s|{{REPO_URL}}|$repo_url|g" \
            "$TEMPLATE_DIR/release-commit.txt" > "$commit_file"
    else
        cat > "$commit_file" <<EOF
chore(release): v$VERSION

Release KDAL v$VERSION — $today.

See CHANGELOG.md for the complete list of changes in this release.
Artifacts: $repo_url/releases/tag/v$VERSION
EOF
    fi

    # Build annotated tag message from template or inline default
    tag_file=$(mktemp /tmp/kdal-tag.XXXXXX)
    if [ -f "$TEMPLATE_DIR/release-tag.txt" ]; then
        sed \
            -e "s/{{VERSION}}/$VERSION/g" \
            -e "s/{{DATE}}/$today/g" \
            -e "s|{{REPO_URL}}|$repo_url|g" \
            "$TEMPLATE_DIR/release-tag.txt" > "$tag_file"
    else
        cat > "$tag_file" <<EOF
KDAL v$VERSION — $today

Kernel Device Abstraction Layer v$VERSION.

Release artifacts (SDK tarballs, .deb, .rpm, .vsix) are published at:
$repo_url/releases/tag/v$VERSION

See CHANGELOG.md for the complete list of changes.
EOF
    fi

    trap 'rm -f "$commit_file" "$tag_file"' EXIT INT TERM

    # ── Dry-run: show what would happen without touching anything ────────────
    if [ "$DRY_RUN" -eq 1 ]; then
        drymsg "Would run: git add -A"
        drymsg "Would run: git commit -F <commit-msg>"
        printf '\n--- Commit message preview ---\n'
        cat "$commit_file"
        printf '\n--- Tag annotation preview ---\n'
        cat "$tag_file"
        printf '\n'
        drymsg "Would run: git tag -a v$VERSION -F <tag-annotation>"
        drymsg "Would run: git push $KDAL_REMOTE $KDAL_BRANCH"
        drymsg "Would run: git push $KDAL_REMOTE v$VERSION"
        rm -f "$commit_file" "$tag_file"
        trap - EXIT INT TERM
        return
    fi

    # ── Stage all changes (version files, CHANGELOG, etc.) ──────────────────
    git add -A

    # Only create a commit if there is something staged
    if git diff --cached --quiet; then
        ok "Nothing to commit — all files already at v$VERSION"
    else
        git commit -F "$commit_file"
        ok "Created release commit"
    fi

    # ── Create annotated tag ─────────────────────────────────────────────────
    git tag -a "v$VERSION" -F "$tag_file"
    ok "Created annotated tag: v$VERSION"

    # ── Push branch and tag ──────────────────────────────────────────────────
    git push "$KDAL_REMOTE" "$KDAL_BRANCH"
    git push "$KDAL_REMOTE" "v$VERSION"
    ok "Pushed $KDAL_BRANCH and tag v$VERSION to $KDAL_REMOTE"

    rm -f "$commit_file" "$tag_file"
    trap - EXIT INT TERM

    printf '\n'
    info "ship: done — GitHub Actions is now building release artifacts"
    printf '  CI:      %s/actions\n' "$repo_url"
    printf '  Release: %s/releases/tag/v%s\n\n' "$repo_url" "$VERSION"
}

# ── post-release ───────────────────────────────────────────────────────────────
cmd_post_release() {
    info "post-release: updating Homebrew formula for v$VERSION"
    cd "$REPO_ROOT"

    if [ "$DRY_RUN" -eq 1 ]; then
        drymsg "Would run: $SCRIPT_DIR/update_homebrew_sha.sh $VERSION"
        drymsg "Would run: git add packaging/homebrew/kdal.rb"
        drymsg "Would run: git commit -m 'chore(homebrew): update formula for v$VERSION'"
        drymsg "Would run: git push $KDAL_REMOTE $KDAL_BRANCH"
        return
    fi

    # Verify the GitHub release is already published before trying to download artifacts.
    info "Checking that release v$VERSION is published on GitHub ..."
    if command -v curl >/dev/null 2>&1; then
        api_url="https://api.github.com/repos/NguyenTrongPhuc552003/kdal/releases/tags/v$VERSION"
        http_code=$(curl -s -o /dev/null -w '%{http_code}' "$api_url")
        [ "$http_code" = "200" ] \
            || die "GitHub release v$VERSION not found (HTTP $http_code). Wait for CI to finish publishing first."
        ok "GitHub release v$VERSION: published"
    else
        warn "curl not available — skipping publish check"
    fi

    "$SCRIPT_DIR/update_homebrew_sha.sh" "$VERSION"

    # Commit the formula update if it actually changed.
    if ! git -C "$REPO_ROOT" diff --quiet packaging/homebrew/kdal.rb; then
        git add packaging/homebrew/kdal.rb
        git commit -m "chore(homebrew): update formula for v$VERSION"
        git push "$KDAL_REMOTE" "$KDAL_BRANCH"
        ok "Homebrew formula committed and pushed"
    else
        ok "Homebrew formula already up to date"
    fi
}

# ── Full pipeline (happy path) ─────────────────────────────────────────────────
cmd_release() {
    info "KDAL release pipeline — v$VERSION"
    [ "$DRY_RUN" -eq 1 ] && info "(dry-run mode: no files or git objects will be mutated)"
    printf '\n'

    cmd_prepare
    printf '\n'
    cmd_notes
    printf '\n'
    cmd_bump
    printf '\n'
    cmd_verify
    printf '\n'
    cmd_ship

    if [ "$DRY_RUN" -eq 0 ]; then
        info "Release v$VERSION complete!"
        printf '\nNext: run post-release once GitHub Actions has published the artifacts:\n'
        printf '  %s post-release %s\n\n' "$(basename "$0")" "$VERSION"
    fi
}

# ── Argument parsing ───────────────────────────────────────────────────────────
VERSION=""
SUBCOMMAND=""

while [ $# -gt 0 ]; do
    case "$1" in
        --dry-run)
            DRY_RUN=1; shift ;;
        -h|--help)
            usage; exit 0 ;;
        prepare|notes|bump|verify|ship)
            SUBCOMMAND="$1"; shift
            # Optional version argument following the subcommand
            if [ $# -gt 0 ] && printf '%s' "$1" | grep -qE '^v?[0-9]'; then
                VERSION="${1#v}"; shift
            fi
            ;;
        post-release)
            SUBCOMMAND="post-release"; shift
            if [ $# -gt 0 ]; then VERSION="${1#v}"; shift; fi
            ;;
        v[0-9]*|[0-9]*)
            SUBCOMMAND="release"
            VERSION="${1#v}"; shift
            ;;
        *)
            die "Unknown argument: $1 — run with --help for usage"
            ;;
    esac
done

if [ -z "$SUBCOMMAND" ]; then
    usage
    exit 1
fi

# Version is required for most subcommands
case "$SUBCOMMAND" in
    prepare|notes|bump|ship|post-release|release)
        [ -n "$VERSION" ] || die "Subcommand '$SUBCOMMAND' requires a <version> argument"
        validate_semver "$VERSION"
        ;;
esac

# Dispatch
case "$SUBCOMMAND" in
    prepare)      cmd_prepare ;;
    notes)        cmd_notes ;;
    bump)         cmd_bump ;;
    verify)       cmd_verify ;;
    ship)         cmd_ship ;;
    post-release) cmd_post_release ;;
    release)      cmd_release ;;
    *)            die "Unknown subcommand: $SUBCOMMAND" ;;
esac
