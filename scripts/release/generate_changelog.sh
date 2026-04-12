#!/bin/sh
# Generate a changelog draft from git history.
#
# Groups commits by type (feat/fix/docs/test/refactor/chore) using
# conventional-commit prefixes.
#
# Usage: ./scripts/release/generate_changelog.sh [since_tag]
# Example: ./scripts/release/generate_changelog.sh v0.1.0

set -eu

SINCE="${1:-}"
DATE=$(date +%Y-%m-%d)

if [ -n "$SINCE" ]; then
	RANGE="$SINCE..HEAD"
	TITLE="Changes since $SINCE"
else
	RANGE="HEAD"
	TITLE="All changes"
fi

echo "# Changelog"
echo ""
echo "## [$TITLE] - $DATE"
echo ""

for type in feat fix docs test refactor chore; do
	commits=$(git log --oneline "$RANGE" --grep="^$type" 2>/dev/null || true)
	if [ -n "$commits" ]; then
		case "$type" in
		feat) echo "### Features" ;;
		fix) echo "### Bug Fixes" ;;
		docs) echo "### Documentation" ;;
		test) echo "### Tests" ;;
		refactor) echo "### Refactoring" ;;
		chore) echo "### Chores" ;;
		esac
		echo ""
		echo "$commits" | while IFS= read -r line; do
			hash=$(echo "$line" | cut -d' ' -f1)
			msg=$(echo "$line" | cut -d' ' -f2-)
			echo "- $msg ($hash)"
		done
		echo ""
	fi
done

# Uncategorized commits
other=$(git log --oneline "$RANGE" \
	--invert-grep --grep="^feat" --grep="^fix" --grep="^docs" \
	--grep="^test" --grep="^refactor" --grep="^chore" 2>/dev/null || true)
if [ -n "$other" ]; then
	echo "### Other"
	echo ""
	echo "$other" | while IFS= read -r line; do
		hash=$(echo "$line" | cut -d' ' -f1)
		msg=$(echo "$line" | cut -d' ' -f2-)
		echo "- $msg ($hash)"
	done
	echo ""
fi
