#!/bin/sh
# E2E Test - Website Build (Astro Starlight documentation)
#
# Simulates: Verifying the documentation website builds correctly,
# all expected pages exist, and the site is deployable.
#
# Usage: ./scripts/e2e/test_website.sh

set -eu

# shellcheck disable=SC2034
E2E_SUITE="website"
. "$(dirname "$0")/lib.sh"

e2e_mktemp
# shellcheck disable=SC2034
SANDBOX="$E2E_TMPDIR"
SITE_DIR="$E2E_ROOT/website"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 1: Website project structure"
# ══════════════════════════════════════════════════════════════════════

assert_dir_exists "$SITE_DIR" "website/"
assert_file_exists "$SITE_DIR/package.json" "package.json"
assert_file_exists "$SITE_DIR/astro.config.mjs" "astro.config.mjs"

# Verify it's a Starlight project
assert_file_contains "$SITE_DIR/package.json" "astro" "depends on astro"
assert_file_contains "$SITE_DIR/package.json" "starlight" "depends on starlight"

# Verify Vercel deployment config
if [ -f "$SITE_DIR/vercel.json" ]; then
	e2e_pass "vercel.json exists"
else
	e2e_skip "vercel.json not found (may use Vercel defaults)"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 2: Content pages"
# ══════════════════════════════════════════════════════════════════════

CONTENT_DIR="$SITE_DIR/src/content/docs"

if [ -d "$CONTENT_DIR" ]; then
	# Count total pages
	_page_count="$(find "$CONTENT_DIR" -name '*.md' -o -name '*.mdx' | wc -l | tr -d ' ')"
	if [ "$_page_count" -ge 20 ]; then
		e2e_pass "documentation has $_page_count pages (>= 20)"
	else
		e2e_fail "only $_page_count pages (expected >= 20)"
	fi

	# Key sections should exist
	for section in getting-started guides reference concepts; do
		if [ -d "$CONTENT_DIR/$section" ]; then
			e2e_pass "section: $section/"
		else
			e2e_skip "section $section/ not found"
		fi
	done

	# Critical pages
	for page in index.md getting-started/installation.md; do
		_path="$CONTENT_DIR/$page"
		if [ -f "$_path" ]; then
			e2e_pass "page: $page"
			assert_file_not_empty "$_path" "$page"
		else
			# Try .mdx
			_mdx_path="${_path%.md}.mdx"
			if [ -f "$_mdx_path" ]; then
				e2e_pass "page: ${page%.md}.mdx"
			else
				e2e_fail "missing page: $page"
			fi
		fi
	done
else
	e2e_fail "content directory not found: $CONTENT_DIR"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 3: Astro configuration"
# ══════════════════════════════════════════════════════════════════════

assert_file_contains "$SITE_DIR/astro.config.mjs" "starlight" "config imports starlight"
assert_file_contains "$SITE_DIR/astro.config.mjs" "title" "config has site title"
assert_file_contains "$SITE_DIR/astro.config.mjs" "sidebar" "config has sidebar"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 4: Static assets"
# ══════════════════════════════════════════════════════════════════════

PUBLIC_DIR="$SITE_DIR/public"
if [ -d "$PUBLIC_DIR" ]; then
	e2e_pass "public/ directory exists"
	# Check for favicon or logo
	if find "$PUBLIC_DIR" -name 'favicon*' -o -name 'logo*' 2>/dev/null | head -1 | grep -q .; then
		e2e_pass "site has favicon/logo"
	else
		e2e_skip "no favicon/logo found in public/"
	fi
else
	e2e_skip "no public/ directory"
fi

# Custom CSS
if find "$SITE_DIR/src" -name '*.css' 2>/dev/null | head -1 | grep -q .; then
	e2e_pass "custom CSS exists"
else
	e2e_skip "no custom CSS found"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 5: npm install and build"
# ══════════════════════════════════════════════════════════════════════

if require_tool npm; then
	# Install dependencies
	if [ ! -d "$SITE_DIR/node_modules" ]; then
		e2e_info "Installing npm dependencies..."
		if (cd "$SITE_DIR" && npm install) >/dev/null 2>&1; then
			e2e_pass "npm install succeeds"
		else
			e2e_fail "npm install failed"
		fi
	else
		e2e_pass "node_modules already present"
	fi

	# Build the site
	if [ -d "$SITE_DIR/node_modules" ]; then
		e2e_info "Building documentation site..."
		if (cd "$SITE_DIR" && npm run build) >/dev/null 2>&1; then
			e2e_pass "npm run build succeeds"

			# Verify build output
			DIST_DIR="$SITE_DIR/dist"
			if [ -d "$DIST_DIR" ]; then
				e2e_pass "dist/ directory created"

				# Count HTML pages
				_html_count="$(find "$DIST_DIR" -name '*.html' | wc -l | tr -d ' ')"
				if [ "$_html_count" -ge 15 ]; then
					e2e_pass "built $_html_count HTML pages (>= 15)"
				else
					e2e_fail "only $_html_count HTML pages (expected >= 15)"
				fi

				# Check index.html
				assert_file_exists "$DIST_DIR/index.html" "dist/index.html"

				# Check for search index (Starlight built-in)
				if find "$DIST_DIR" -name 'pagefind*' -o -name '*search*' 2>/dev/null | head -1 | grep -q .; then
					e2e_pass "search index generated"
				else
					e2e_skip "no search index found"
				fi
			else
				e2e_fail "dist/ not created after build"
			fi
		else
			e2e_fail "npm run build failed"
		fi
	else
		e2e_skip "dependencies not installed - skip build"
	fi
else
	e2e_skip "npm not available - skip website build test"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 6: docs.yml workflow"
# ══════════════════════════════════════════════════════════════════════

DOCS_YML="$E2E_ROOT/.github/workflows/docs.yml"

if [ -f "$DOCS_YML" ]; then
	e2e_pass "docs.yml workflow exists"
	assert_file_contains "$DOCS_YML" "npm" "docs workflow uses npm"
	assert_file_contains "$DOCS_YML" "build" "docs workflow runs build"
	assert_file_contains "$DOCS_YML" "vercel\|deploy\|pages" "docs workflow deploys"
else
	e2e_fail "docs.yml workflow not found"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_summary
