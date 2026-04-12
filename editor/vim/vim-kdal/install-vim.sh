#!/bin/sh
# Install KDAL Vim/Neovim syntax files.
#
# Usage:
#   ./install-vim.sh              # Install to ~/.vim (user-local)
#   ./install-vim.sh --system     # Install to /usr/share/vim/vimfiles
#   ./install-vim.sh --neovim     # Install to Neovim stdpath
#   ./install-vim.sh --uninstall  # Remove installed files
#
# KDAL SDK installer (kdalup) calls this script automatically.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VIM_KDAL_DIR="${SCRIPT_DIR}"

info()  { printf "\033[1;34m==> %s\033[0m\n" "$1"; }
warn()  { printf "\033[1;33m==> %s\033[0m\n" "$1"; }
error() { printf "\033[1;31mError: %s\033[0m\n" "$1"; exit 1; }

# Determine target directory
TARGET=""
UNINSTALL=false

for arg in "$@"; do
    case "$arg" in
        --system)    TARGET="/usr/share/vim/vimfiles" ;;
        --neovim)
            if command -v nvim >/dev/null 2>&1; then
                nvim_data="$(nvim --headless -c 'echo stdpath("data")' -c 'qa' 2>&1 | tr -d '\n')"
                TARGET="${nvim_data}/site"
            else
                TARGET="${HOME}/.local/share/nvim/site"
            fi
            ;;
        --uninstall) UNINSTALL=true ;;
        *)           error "Unknown option: $arg" ;;
    esac
done

# Default: user-local Vim
if [ -z "$TARGET" ]; then
    TARGET="${HOME}/.vim"
fi

DIRS="ftdetect syntax ftplugin compiler plugin"
FILES_INSTALLED=""

if [ "$UNINSTALL" = true ]; then
    info "Uninstalling KDAL Vim files from ${TARGET}"

    for d in $DIRS; do
        for f in "${VIM_KDAL_DIR}/${d}"/*.vim; do
            [ -f "$f" ] || continue
            base="$(basename "$f")"
            dest="${TARGET}/${d}/${base}"
            if [ -f "$dest" ]; then
                rm -f "$dest"
                echo "  removed ${dest}"
            fi
        done
    done

    # filetype.lua for Neovim
    if [ -f "${TARGET}/filetype.lua" ]; then
        # Only remove if it's ours (check for kdh marker)
        if grep -q 'kdh' "${TARGET}/filetype.lua" 2>/dev/null; then
            rm -f "${TARGET}/filetype.lua"
            echo "  removed ${TARGET}/filetype.lua"
        fi
    fi

    info "Uninstall complete"
    exit 0
fi

info "Installing KDAL Vim files to ${TARGET}"

for d in $DIRS; do
    mkdir -p "${TARGET}/${d}"
    for f in "${VIM_KDAL_DIR}/${d}"/*.vim; do
        [ -f "$f" ] || continue
        base="$(basename "$f")"
        cp "$f" "${TARGET}/${d}/${base}"
        echo "  ${TARGET}/${d}/${base}"
    done
done

# filetype.lua for Neovim
if [ -f "${VIM_KDAL_DIR}/filetype.lua" ]; then
    cp "${VIM_KDAL_DIR}/filetype.lua" "${TARGET}/filetype.lua"
    echo "  ${TARGET}/filetype.lua"
fi

info "Install complete - restart Vim/Neovim to activate"
