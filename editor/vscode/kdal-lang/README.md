# KDAL Language Support for VS Code

Full IDE experience for the **Kernel Device Abstraction Layer** - write,
lint, compile, and simulate KDAL device headers (`.kdh`) and driver files
(`.kdc`) directly from VS Code.

## Quick Start (2 Steps)

**Step 1 - Install the KDAL SDK:**

```sh
curl -fsSL https://raw.githubusercontent.com/NguyenTrongPhuc552003/kdal/main/scripts/installer/install.sh | sh
```

**Step 2 - Install this extension:**

```sh
code --install-extension kdal-lang-0.1.0.vsix
```

That's it. The extension auto-detects `kdalc` and `kdality` from `KDAL_HOME/bin` or `PATH`. Open any `.kdh` or `.kdc` file to start.

## Features

- **Syntax highlighting** for `.kdh` and `.kdc` (aligned with the KDAL EBNF grammar)
- **Code snippets** for rapid driver scaffolding (20+ snippets)
- **Compile** `.kdc` drivers to kernel C modules (via `kdality compile`)
- **Lint on save** - automatic diagnostics reported in the Problems panel
- **Simulate** `.kdh` devices (via `kdality simulate`)
- **Generate device trees** from `.kdh` files (via `kdality dt-gen`)
- **Generate tests** from `.kdc` drivers (via `kdality test-gen`)
- **Create new KDAL projects** from the command palette (via `kdality init`)
- **Status bar** - shows KDAL toolchain status and active file type
- **Build tasks** - auto-detected KDAL tasks in the task runner
- **File icons** - distinct icons for `.kdh` and `.kdc` in the explorer
- **Context menus** - right-click actions for KDAL files
- **Guided walkthrough** - step-by-step onboarding for new users

## Commands

| Command                      | Description                        | Keybinding |
| ---------------------------- | ---------------------------------- | ---------- |
| `KDAL: Compile Driver`       | Compile active `.kdc` file         | -          |
| `KDAL: Lint File`            | Lint active `.kdh` or `.kdc` file  | -          |
| `KDAL: Create New Project`   | Scaffold a new KDAL driver project | -          |
| `KDAL: Simulate Device`      | Run register simulation on `.kdh`  | -          |
| `KDAL: Generate Device Tree` | Generate DT overlay from `.kdh`    | -          |
| `KDAL: Generate Test`        | Generate KUnit test from `.kdc`    | -          |
| `KDAL: Show Toolchain Info`  | Display kdalc / kdality versions   | -          |
| `KDAL: Install SDK`          | Download and install the KDAL SDK  | -          |
| `KDAL: Re-detect Toolchain`  | Re-scan for kdalc and kdality      | -          |

> **No SDK?** The extension shows a notification on activation with an
> **Install SDK** button that runs `kdalup` in a terminal. The status bar
> also offers one-click install when the toolchain is missing.

## Settings

| Setting              | Default | Description                                    |
| -------------------- | ------- | ---------------------------------------------- |
| `kdal.toolchainPath` | `""`    | Path to directory containing kdalc and kdality |
| `kdal.stdlibPath`    | `""`    | Path to KDAL standard library (.kdh files)     |
| `kdal.lintOnSave`    | `true`  | Automatically lint .kdh/.kdc files on save     |

## Snippets

### `.kdh` files

| Prefix     | Description             |
| ---------- | ----------------------- |
| `device`   | Full device skeleton    |
| `reg`      | Register map entry      |
| `sig`      | Signal declaration      |
| `bitfield` | Bitfield register entry |
| `import`   | Import a stdlib header  |
| `config`   | Configuration option    |

### `.kdc` files

| Prefix     | Description            |
| ---------- | ---------------------- |
| `driver`   | Full driver skeleton   |
| `onprobe`  | Probe handler          |
| `onremove` | Remove handler         |
| `onread`   | Read handler           |
| `onwrite`  | Write handler          |
| `onsignal` | Signal handler         |
| `onpower`  | Power state handler    |
| `rr`       | `reg_read()`           |
| `rw`       | `reg_write()`          |
| `log`      | `log()` statement      |
| `if`       | Conditional block      |
| `for`      | Loop statement         |
| `backend`  | Backend-specific block |

## Known Issues

- **v0.1.0**: QEMU launch/deploy/debug integration is not yet available - use `scripts/env/` scripts directly
- **v0.1.0**: No real hardware validation - QEMU aarch64 only
- Extension screenshots will be added in a future update

## Roadmap

- **v0.2.0**: QEMU integration (launch VM, deploy module, attach debugger from VS Code)
- **v0.2.0**: Keybindings for compile/lint/simulate
- **v0.3.0**: Language server protocol (LSP) for go-to-definition and hover docs

## Development

```sh
cd editor/vscode/kdal-lang
npm install
npm run compile
# Press F5 in VS Code to launch the Extension Development Host
```

## Packaging

```sh
npm run package   # produces kdal-lang-x.y.z.vsix
```

## License

GPL-3.0-or-later - see [LICENSE](https://github.com/NguyenTrongPhuc552003/kdal/blob/main/LICENSE).
