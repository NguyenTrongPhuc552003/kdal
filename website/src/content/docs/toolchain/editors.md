---
title: Editor Support
description: IDE and editor integrations for KDAL development.
---

KDAL ships first-party editor support for **VS Code** and **Vim/Neovim**.
Both are installed automatically by `kdalup` and bundled in release tarballs.

---

## VS Code Extension

The `kdal-lang` extension provides a full IDE experience for `.kdh` and `.kdc` files.

### Installation

Install from the VS Code Marketplace:

```
ext install kdal-project.kdal-lang
```

Or via `kdalup`:

```bash
kdalup install          # installs SDK + VS Code extension
kdalup component add vscode
```

### Features

| Feature                 | Description                                                                                                                             |
| ----------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| **Syntax highlighting** | Separate TextMate grammars for `.kdh` and `.kdc`                                                                                        |
| **Code snippets**       | `.kdh`: `device`, `reg`, `sig` - `.kdc`: `driver`, `onprobe`, `onremove`, `onread`, `onwrite`, `onsignal`, `onpower`, `rr`, `rw`, `log` |
| **Lint on save**        | Runs `kdality lint` automatically (configurable)                                                                                        |
| **File icons**          | Distinct icons for `.kdh` and `.kdc`                                                                                                    |
| **Status bar**          | Shows toolchain version and status                                                                                                      |
| **Build tasks**         | Auto-detected task definitions for compile/lint/simulate                                                                                |
| **Context menus**       | Right-click actions scoped by file language                                                                                             |

### Commands

| Command                        | Shortcut                 | Scope             |
| ------------------------------ | ------------------------ | ----------------- |
| **KDAL: Compile Driver**       | `kdal.compile`           | `.kdc` files      |
| **KDAL: Lint File**            | `kdal.lint`              | `.kdh` and `.kdc` |
| **KDAL: Create New Project**   | `kdal.init`              | Always            |
| **KDAL: Simulate Device**      | `kdal.simulate`          | `.kdh` files      |
| **KDAL: Generate Device Tree** | `kdal.dtgen`             | `.kdh` files      |
| **KDAL: Generate Test**        | `kdal.testgen`           | `.kdc` files      |
| **KDAL: Show Toolchain Info**  | `kdal.showToolchainInfo` | Always            |

### Settings

| Setting              | Default | Description                        |
| -------------------- | ------- | ---------------------------------- |
| `kdal.toolchainPath` | `""`    | Path to `kdalc`/`kdality` binaries |
| `kdal.stdlibPath`    | `""`    | Path to stdlib `.kdh` files        |
| `kdal.lintOnSave`    | `true`  | Run linter automatically on save   |

### Task Definitions

The extension registers a `kdal` task type. Example `.vscode/tasks.json`:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "type": "kdal",
      "command": "compile",
      "file": "${file}",
      "label": "KDAL: Compile current file"
    }
  ]
}
```

Supported commands: `compile`, `lint`, `simulate`, `dt-gen`, `test-gen`.

---

## Vim / Neovim Plugin

`vim-kdal` provides syntax highlighting, filetype detection, compiler
integration, and code folding.

### Installation

Choose your preferred plugin manager:

**vim-plug:**

```vim
Plug 'NguyenTrongPhuc552003/kdal', { 'rtp': 'editor/vim/vim-kdal' }
```

**lazy.nvim:**

```lua
{
  'NguyenTrongPhuc552003/kdal',
  config = function()
    vim.opt.rtp:append(
      vim.fn.stdpath('data') .. '/lazy/kdal/editor/vim/vim-kdal'
    )
  end,
}
```

**Pathogen:**

```bash
cd ~/.vim/bundle
git clone https://github.com/NguyenTrongPhuc552003/kdal.git
# Add to .vimrc:
# set rtp+=~/.vim/bundle/kdal/editor/vim/vim-kdal
```

**Manual / System-wide:**

```bash
cd editor/vim/vim-kdal
./install-vim.sh          # copies to ~/.vim/
# or via kdalup:
kdalup component add vim
```

### Features

| Feature                 | Description                                                                   |
| ----------------------- | ----------------------------------------------------------------------------- |
| **Syntax highlighting** | Full EBNF grammar coverage for `.kdh` and `.kdc`                              |
| **Filetype detection**  | Automatic for `.kdh` and `.kdc` (Vim + Neovim ≥ 0.8 `filetype.lua`)           |
| **Comment toggling**    | Works with `gc` (vim-commentary)                                              |
| **Smart indentation**   | 4-space indent with `smartindent`                                             |
| **Syntax folding**      | Fold on brace blocks with `zc`/`zo`                                           |
| **Compiler plugins**    | `:compiler kdality` (lint → quickfix), `:compiler kdalc` (compile → quickfix) |

### Compiler Integration

```vim
" Lint the current file and jump to first warning
:compiler kdality
:make %

" Compile the current driver and jump to errors
:compiler kdalc
:make %
```

Errors and warnings populate the quickfix list (`:copen`).

### Plugin Files

| Path                   | Purpose                             |
| ---------------------- | ----------------------------------- |
| `ftdetect/kdal.vim`    | Filetype detection (`.kdh`, `.kdc`) |
| `syntax/kdh.vim`       | `.kdh` syntax rules                 |
| `syntax/kdc.vim`       | `.kdc` syntax rules                 |
| `ftplugin/kdh.vim`     | `.kdh` comment/indent settings      |
| `ftplugin/kdc.vim`     | `.kdc` comment/indent settings      |
| `compiler/kdality.vim` | `:compiler kdality` - lint          |
| `compiler/kdalc.vim`   | `:compiler kdalc` - compile         |
| `plugin/kdal.vim`      | Plugin entry point                  |
| `filetype.lua`         | Neovim ≥ 0.8 detection              |
