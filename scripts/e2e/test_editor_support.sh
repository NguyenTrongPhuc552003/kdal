#!/bin/sh
# E2E Test - Editor Support (VS Code + Vim/Neovim)
#
# Simulates: A developer setting up their editor for KDAL development.
# Validates VS Code extension package, Vim plugin structure, syntax
# files, and filetype detection.
#
# Usage: ./scripts/e2e/test_editor_support.sh

set -eu

# shellcheck disable=SC2034
E2E_SUITE="editor-support"
. "$(dirname "$0")/lib.sh"

e2e_mktemp
SANDBOX="$E2E_TMPDIR"

VSCODE_DIR="$E2E_ROOT/editor/vscode/kdal-lang"
VIM_DIR="$E2E_ROOT/editor/vim/vim-kdal"

# ══════════════════════════════════════════════════════════════════════
e2e_section "VS Code Extension - Package Structure"
# ══════════════════════════════════════════════════════════════════════

assert_file_exists "$VSCODE_DIR/package.json" "package.json"
assert_file_exists "$VSCODE_DIR/tsconfig.json" "tsconfig.json"
assert_file_exists "$VSCODE_DIR/README.md" "extension README"

# package.json validation
assert_file_contains "$VSCODE_DIR/package.json" '"name": "kdal-lang"' "package name"
assert_file_contains "$VSCODE_DIR/package.json" '"publisher": "phuctrong552003"' "publisher"
assert_file_contains "$VSCODE_DIR/package.json" '"version"' "has version field"
assert_file_contains "$VSCODE_DIR/package.json" '"main": "./out/extension.js"' "main entry"
assert_file_contains "$VSCODE_DIR/package.json" '"engines"' "VS Code engine constraint"

# ══════════════════════════════════════════════════════════════════════
e2e_section "VS Code Extension - Language Definitions"
# ══════════════════════════════════════════════════════════════════════

# Grammar files
assert_file_exists "$VSCODE_DIR/syntaxes/kdh.tmLanguage.json" "kdh grammar"
assert_file_exists "$VSCODE_DIR/syntaxes/kdc.tmLanguage.json" "kdc grammar"

# Validate JSON syntax of grammar files
for gram in "$VSCODE_DIR/syntaxes/kdh.tmLanguage.json" \
	"$VSCODE_DIR/syntaxes/kdc.tmLanguage.json"; do
	if require_tool python3; then
		if python3 -m json.tool "$gram" >/dev/null 2>&1; then
			e2e_pass "$(basename "$gram") is valid JSON"
		else
			e2e_fail "$(basename "$gram") is invalid JSON"
		fi
	elif require_tool node; then
		if node -e "JSON.parse(require('fs').readFileSync('$gram','utf8'))" 2>/dev/null; then
			e2e_pass "$(basename "$gram") is valid JSON"
		else
			e2e_fail "$(basename "$gram") is invalid JSON"
		fi
	fi
done

# Scope names
assert_file_contains "$VSCODE_DIR/syntaxes/kdh.tmLanguage.json" \
	"source.kdh" "kdh scope name"
assert_file_contains "$VSCODE_DIR/syntaxes/kdc.tmLanguage.json" \
	"source.kdc" "kdc scope name"

# Language configuration
assert_file_exists "$VSCODE_DIR/language-configuration.json" "language configuration"

# ══════════════════════════════════════════════════════════════════════
e2e_section "VS Code Extension - Snippets"
# ══════════════════════════════════════════════════════════════════════

assert_file_exists "$VSCODE_DIR/snippets/kdh.json" "kdh snippets"
assert_file_exists "$VSCODE_DIR/snippets/kdc.json" "kdc snippets"

for snip in "$VSCODE_DIR/snippets/kdh.json" "$VSCODE_DIR/snippets/kdc.json"; do
	if [ -f "$snip" ]; then
		if require_tool python3; then
			if python3 -m json.tool "$snip" >/dev/null 2>&1; then
				e2e_pass "$(basename "$snip") is valid JSON"
			else
				e2e_fail "$(basename "$snip") is invalid JSON"
			fi
		fi
	fi
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "VS Code Extension - TypeScript Sources"
# ══════════════════════════════════════════════════════════════════════

assert_dir_exists "$VSCODE_DIR/src" "src/ directory"
assert_file_exists "$VSCODE_DIR/src/extension.ts" "extension.ts"

# Check for key extension features
assert_file_contains "$VSCODE_DIR/src/extension.ts" "activate" "activate function"

# Count TypeScript source files
assert_file_count "$VSCODE_DIR/src" "*.ts" 3 "TypeScript source files"

# ══════════════════════════════════════════════════════════════════════
e2e_section "VS Code Extension - Commands"
# ══════════════════════════════════════════════════════════════════════

# Verify all expected commands are in package.json
for cmd in "kdal.compile" "kdal.lint" "kdal.init" "kdal.simulate" \
	"kdal.dtgen" "kdal.testgen" "kdal.showToolchainInfo"; do
	assert_file_contains "$VSCODE_DIR/package.json" "$cmd" "command: $cmd"
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "VS Code Extension - TypeScript compilation"
# ══════════════════════════════════════════════════════════════════════

if [ -f "$VSCODE_DIR/package.json" ] && require_tool npm; then
	# the locked dependencies (package-lock.json); a failed install fails
	if [ ! -x "$VSCODE_DIR/node_modules/.bin/tsc" ]; then
		e2e_info "Installing npm dependencies (npm ci)..."
		if ! (cd "$VSCODE_DIR" && npm ci --ignore-scripts) >"$SANDBOX/npm-ci.log" 2>&1; then
			e2e_fail "npm ci: $(tail -3 "$SANDBOX/npm-ci.log" | tr '\n' ' ')"
		fi
	fi

	if [ -x "$VSCODE_DIR/node_modules/.bin/tsc" ]; then
		# the project's own TypeScript, not one found elsewhere on PATH
		if (cd "$VSCODE_DIR" && ./node_modules/.bin/tsc --noEmit) >"$SANDBOX/tsc.log" 2>&1; then
			e2e_pass "TypeScript compiles without errors"
		else
			e2e_fail "TypeScript compilation errors: $(head -3 "$SANDBOX/tsc.log" | tr '\n' ' ')"
		fi
	fi
else
	e2e_skip "npm not available - skip TS compilation"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "VS Code Extension - VSIX packaging"
# ══════════════════════════════════════════════════════════════════════

if require_tool npx && [ -x "$VSCODE_DIR/node_modules/.bin/vsce" ]; then
	if (cd "$VSCODE_DIR" && ./node_modules/.bin/vsce package --no-dependencies -o "$SANDBOX/kdal-lang.vsix") \
		>/dev/null 2>&1; then
		e2e_pass "VSIX packages successfully"
		assert_file_not_empty "$SANDBOX/kdal-lang.vsix" "kdal-lang.vsix"
	else
		e2e_fail "VSIX packaging (npx vsce package)"
	fi
else
	e2e_skip "vsce not available - skip VSIX test"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Vim/Neovim Plugin - Structure"
# ══════════════════════════════════════════════════════════════════════

assert_dir_exists "$VIM_DIR" "vim-kdal/"
assert_file_exists "$VIM_DIR/README.md" "vim plugin README"

# Required directories
for vdir in syntax ftdetect ftplugin compiler plugin; do
	assert_dir_exists "$VIM_DIR/$vdir" "vim-kdal/$vdir/"
done

# Neovim filetype.lua
assert_file_exists "$VIM_DIR/filetype.lua" "filetype.lua (Neovim)"

# Install script
assert_file_exists "$VIM_DIR/install-vim.sh" "install-vim.sh"
assert_cmd "install-vim.sh is valid shell" sh -n "$VIM_DIR/install-vim.sh"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Vim/Neovim Plugin - Syntax Files"
# ══════════════════════════════════════════════════════════════════════

# Syntax highlighting
for ext in kdh kdc; do
	assert_file_exists "$VIM_DIR/syntax/${ext}.vim" "syntax/${ext}.vim"
	assert_file_contains "$VIM_DIR/syntax/${ext}.vim" "syn " "${ext}.vim has syntax definitions"
done

# Filetype detection
# one file, ftdetect/kdal.vim, sets both filetypes
assert_file_exists "$VIM_DIR/ftdetect/kdal.vim" "ftdetect/kdal.vim"
for ext in kdh kdc; do
	assert_file_contains "$VIM_DIR/ftdetect/kdal.vim" "\\.${ext}" "ftdetect/kdal.vim detects .${ext}"
done

# ftplugin
for ext in kdh kdc; do
	ftp="$VIM_DIR/ftplugin/${ext}.vim"
	if [ -f "$ftp" ]; then
		e2e_pass "ftplugin/${ext}.vim exists"
	else
		e2e_skip "ftplugin/${ext}.vim"
	fi
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "Vim/Neovim Plugin - Neovim filetype.lua"
# ══════════════════════════════════════════════════════════════════════

assert_file_contains "$VIM_DIR/filetype.lua" "kdh" "filetype.lua handles .kdh"
assert_file_contains "$VIM_DIR/filetype.lua" "kdc" "filetype.lua handles .kdc"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Vim/Neovim Plugin - Compiler Integration"
# ══════════════════════════════════════════════════════════════════════

COMPILER_VIM="$VIM_DIR/compiler"
if [ -d "$COMPILER_VIM" ]; then
	assert_file_count "$COMPILER_VIM" "*.vim" 1 "compiler/*.vim files"
	# Should reference kdality or kdalc
	for cv in "$COMPILER_VIM"/*.vim; do
		if [ -f "$cv" ]; then
			assert_file_contains "$cv" "kdalc\|kdality" \
				"$(basename "$cv") references KDAL tools"
		fi
	done
else
	e2e_skip "compiler/ directory not present"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Vim/Neovim Plugin - Simulated install"
# ══════════════════════════════════════════════════════════════════════

VIM_INSTALL_DIR="$SANDBOX/vim-install"
mkdir -p "$VIM_INSTALL_DIR"

# Simulate manual install (copy files)
for sub in syntax ftdetect ftplugin compiler plugin; do
	if [ -d "$VIM_DIR/$sub" ]; then
		mkdir -p "$VIM_INSTALL_DIR/$sub"
		cp "$VIM_DIR/$sub"/* "$VIM_INSTALL_DIR/$sub/" 2>/dev/null || true
	fi
done

if [ -f "$VIM_DIR/filetype.lua" ]; then
	cp "$VIM_DIR/filetype.lua" "$VIM_INSTALL_DIR/"
fi

# Verify copied structure
assert_file_count "$VIM_INSTALL_DIR" "*.vim" 3 "installed vim files"

# ══════════════════════════════════════════════════════════════════════
e2e_summary
