#!/bin/sh
# The quality gate: haven's engine (tools/gate/) at the commit pinned in
# quality/gate.json, fetched once into the user's cache, run on this tree.
# Usage: scripts/gate pre-commit | pre-push | ci | deep | baseline | doctor
#        scripts/gate engine-dir   (fetch, then print the engine's directory)
# Design: https://github.com/NguyenTrongPhuc552003/haven/blob/main/docs/quality/gate.md
set -eu
root="$(cd "$(dirname "$0")/.." && pwd)"
pin="$(python3 -c 'import json,sys;e=json.load(open(sys.argv[1]))["engine"];print(e["repo"], e["commit"])' \
	"$root/quality/gate.json")"
repo="${pin% *}"
commit="${pin#* }"
case "$commit" in
*[!0-9a-f]* | "") echo "gate: engine.commit in quality/gate.json is not a commit: '$commit'" >&2; exit 2 ;;
esac
cache="${XDG_CACHE_HOME:-$HOME/.cache}/haven-gate/$commit"
if [ ! -f "$cache/tools/gate/gate.py" ]; then
	tmp="$cache.tmp.$$"
	rm -rf "$tmp"
	mkdir -p "$tmp"
	if ! { git -C "$tmp" init -q && git -C "$tmp" fetch -q --depth 1 "$repo" "$commit" &&
		git -C "$tmp" checkout -q FETCH_HEAD -- tools/gate; } >/dev/null 2>&1; then
		rm -rf "$tmp"
		echo "gate: cannot fetch the engine at $commit from $repo (engine in quality/gate.json)" >&2
		exit 2
	fi
	rm -rf "$tmp/.git"
	mv "$tmp" "$cache" 2>/dev/null || rm -rf "$tmp"
fi
if [ "${1:-}" = engine-dir ]; then
	echo "$cache"
	exit 0
fi
exec python3 "$cache/tools/gate/gate.py" --root "$root" "$@"
