# vim-kdal

Vim and Neovim syntax highlighting for the **Kernel Device Abstraction Layer** (KDAL).

Provides syntax highlighting, filetype detection, comment/indent settings,
and compiler integration for `.kdh` (device headers) and `.kdc` (driver code) files.

## Features

- **Syntax highlighting** - Full coverage of KDAL EBNF grammar: keywords, types,
  access qualifiers, built-in functions, signal/power constructs, operators
- **Filetype detection** - `.kdh` and `.kdc` files are auto-detected
- **Comment toggling** - Works with `gc` (vim-commentary) and built-in `formatoptions`
- **Smart indentation** - 4-space indentation, `smartindent` enabled
- **Syntax folding** - Fold on brace blocks (`zc`, `zo`)
- **Compiler plugins** - `:compiler kdality` for lint, `:compiler kdalc` for compile,
  results in the quickfix list (`:copen`)

## Installation

### vim-plug

```vim
Plug 'NguyenTrongPhuc552003/kdal', { 'rtp': 'editor/vim/vim-kdal' }
```

### lazy.nvim (Neovim)

```lua
{
  "NguyenTrongPhuc552003/kdal",
  config = function()
    vim.opt.rtp:append(vim.fn.stdpath("data") .. "/lazy/kdal/editor/vim/vim-kdal")
  end,
}
```

### Pathogen

```sh
cd ~/.vim/bundle
git clone https://github.com/NguyenTrongPhuc552003/kdal.git
# Add to .vimrc:  set rtp+=~/.vim/bundle/kdal/editor/vim/vim-kdal
```

### Manual / System-wide

```sh
# Copy to Vim's runtime directory
cp -r editor/vim/vim-kdal/* ~/.vim/

# Or system-wide (requires root)
sudo cp -r editor/vim/vim-kdal/* /usr/share/vim/vimfiles/

# Or via KDAL SDK installer (kdalup handles this automatically)
```

## Usage

Open any `.kdh` or `.kdc` file - syntax highlighting is automatic.

### Lint with quickfix

```vim
:compiler kdality
:make
:copen
```

### Compile with quickfix

```vim
:compiler kdalc
:make
:copen
```

## File Structure

```
vim-kdal/
├── ftdetect/
│   └── kdal.vim         # Filetype detection (.kdh, .kdc)
├── syntax/
│   ├── kdh.vim          # Syntax highlighting for .kdh
│   └── kdc.vim          # Syntax highlighting for .kdc
├── ftplugin/
│   ├── kdh.vim          # Comment/indent settings for .kdh
│   └── kdc.vim          # Comment/indent settings for .kdc
├── compiler/
│   ├── kdality.vim      # :make → kdality lint
│   └── kdalc.vim        # :make → kdality compile
├── plugin/
│   └── kdal.vim         # Plugin entry point
├── filetype.lua         # Neovim >= 0.8 filetype detection
└── README.md
```

## License

GPL-3.0-or-later - see [LICENSE](https://github.com/NguyenTrongPhuc552003/kdal/blob/main/LICENSE).
