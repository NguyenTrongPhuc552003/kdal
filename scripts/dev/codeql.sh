#!/bin/sh
# Run CodeQL analysis locally before committing.
# Usage: scripts/dev/codeql.sh [language]
#   language: cpp (default), javascript, python
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
. "${SCRIPT_DIR}/common.sh"

LANG="${1:-cpp}"
DB_DIR=".codeql-db"
RESULTS_FILE="codeql-results.sarif"
CPP_PRESET="${KDAL_CODEQL_PRESET:-ci-release}"

if ! command -v codeql >/dev/null 2>&1; then
	echo "error: codeql CLI not found. Install with: brew install codeql"
	exit 1
fi

echo "==> Creating CodeQL database for ${LANG}..."
rm -rf "$DB_DIR"

if [ "$LANG" = "cpp" ]; then
	BUILD_DIR="$(dev_build_dir_for_preset "${REPO_ROOT}" "${CPP_PRESET}")"
	echo "==> Removing previous C/C++ build tree: ${BUILD_DIR}"
	rm -rf "$BUILD_DIR"

	BUILD_CMD="scripts/dev/build.sh --preset ${CPP_PRESET}"
	codeql database create "$DB_DIR" \
		--language=cpp \
		--command="$BUILD_CMD" \
		--overwrite
else
	codeql database create "$DB_DIR" \
		--language="$LANG" \
		--overwrite
fi

echo "==> Running analysis..."
echo "==> Ensuring query pack codeql/${LANG}-queries is installed..."
codeql pack download "codeql/${LANG}-queries" >/dev/null

codeql database analyze "$DB_DIR" \
	--format=sarif-latest \
	--output="$RESULTS_FILE" \
	"codeql/${LANG}-queries:codeql-suites/${LANG}-security-and-quality.qls" \
	2>&1

FINDINGS=$(grep -o '"ruleId"' "$RESULTS_FILE" 2>/dev/null | wc -l | tr -d ' ')
echo ""
echo "==> Done: ${FINDINGS} finding(s) in ${RESULTS_FILE}"

if [ "$FINDINGS" -gt 0 ]; then
	echo ""
	echo "Top issues:"
	grep -o '"ruleId" *: *"[^"]*"' "$RESULTS_FILE" | sort | uniq -c | sort -rn | head -10
	echo ""
	echo "Full results: ${RESULTS_FILE}"
	echo "View in VS Code: install the SARIF Viewer extension"
fi
