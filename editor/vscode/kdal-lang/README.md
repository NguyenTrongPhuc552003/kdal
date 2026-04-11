# KDAL Language Support for VS Code

Syntax highlighting and code snippets for KDAL device headers (`.kdh`) and
driver files (`.kdc`).

## Features

- Syntax highlighting for `.kdh` (device headers) and `.kdc` (driver code)
- Code snippets for rapid driver development
- Comment toggling, bracket matching, auto-closing pairs

## Installation (Development)

1. Copy or symlink this folder into `~/.vscode/extensions/`:
   ```sh
   ln -s $(pwd) ~/.vscode/extensions/kdal-lang
   ```
2. Restart VS Code
3. Open any `.kdh` or `.kdc` file

## Snippets

### `.kdh` files

| Prefix   | Description          |
| -------- | -------------------- |
| `device` | Full device skeleton |
| `reg`    | Register map entry   |
| `sig`    | Signal declaration   |

### `.kdc` files

| Prefix     | Description          |
| ---------- | -------------------- |
| `driver`   | Full driver skeleton |
| `onprobe`  | Probe handler        |
| `onremove` | Remove handler       |
| `onread`   | Read handler         |
| `onwrite`  | Write handler        |
| `onsignal` | Signal handler       |
| `onpower`  | Power state handler  |
| `rr`       | `reg_read()`         |
| `rw`       | `reg_write()`        |
| `log`      | `log()` statement    |
