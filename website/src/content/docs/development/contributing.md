---
title: Contributing
description: Guidelines for contributing to the KDAL project.
---

KDAL is a master thesis project with production-grade engineering standards.
Contributions must preserve API clarity, QEMU-first reproducibility, and
Linux kernel coding discipline.

## Development Setup

```bash
# Install build tools
./scripts/env/dependencies.sh

# Build everything (compiler + tool)
./scripts/dev/build.sh --variant debug all

# Prepare the QEMU workspace when you need guest-side validation
./scripts/env/prepare.sh
./scripts/env/kernel.sh

# Run CI suite
./scripts/ci/test.sh

# Install the git hooks (once per clone), then run the quality gate
scripts/gate install
scripts/gate pre-push
```

## Quality Gate

KDAL uses the quality gate shared by the HAVEN ecosystem
([design](https://github.com/NguyenTrongPhuc552003/haven/blob/main/docs/quality/gate.md)).
`scripts/gate` fetches the gate's engine from haven at the commit pinned in
`quality/gate.json` (once, into `~/.cache/haven-gate/`) and runs it with
KDAL's profile:

| Stage | When | Checks |
| - | - | - |
| `pre-commit` | `git commit` (hook) | secrets, personal names and home paths in the staged files |
| `pre-push` | `git push` (hook) | checkpatch.pl (Linux v6.18) on the kernel module, cppcheck, clang-tidy on `compiler/` and `tools/kdality/`, lizard, jscpd, shellcheck, ruff on the files changed against `main` |
| `ci` | every pull request, required | the same on the whole tree, plus gitleaks |

Findings are rated BLOCKER, CRITICAL, MAJOR, MINOR or INFO. A BLOCKER always
stops the change; a CRITICAL or MAJOR finding stops it when it is not in
`quality/baseline.json`. The baseline holds the findings that existed when
the gate started: fix them when you can, and after fixing run
`scripts/gate baseline` and commit the smaller baseline. Never add new
findings to the baseline to get a change through. `scripts/gate doctor`
lists the tools found; `tools/gate/ci-setup.sh` in the engine's directory
(`scripts/gate engine-dir`) installs the pinned versions. A newer engine is
taken by changing `engine.commit` in a pull request of its own.

## Code Style

- **Kernel code**: Linux kernel coding style (`checkpatch.pl` clean)
  - Tabs for indentation, 80-column soft limit
  - `/* C89 comments */`
  - `kdal_` prefix for all exported symbols
  - Proper `goto` error unwinding
- **Userspace code**: C99 with `-Wall -Wextra -pedantic`
- **Scripts**: Bash with `set -euo pipefail`

## Commit Messages

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
feat(driver): add GPIO driver class
fix(chardev): handle partial write correctly
test(kunit): add registry overflow test
docs(api): document new ioctl command
ci: add kernel build matrix
```

Rules:
- Separate refactors from behavior changes
- One logical change per commit
- Explain **why**, not just **what**
- Call out ABI or API impact explicitly

## Pull Request Process

1. Fork and create a feature branch
2. Implement changes with tests
3. Run `./scripts/ci/test.sh` locally
4. Push with the hooks installed (`scripts/gate install`), so the gate checks your changes
5. Submit PR with description of changes and testing done

## Architecture Rules

- **No backend-specific types in public headers** (`include/kdal/`)
- **No direct hardware access in core or drivers** - always through backend ops
- **No new ioctl without version bump** - see `scripts/release/bump_version.sh`
- **No `printk` spam** - use `pr_debug()` for development, `pr_info()` for lifecycle
- **No sleeping in registry iteration** - callbacks must be non-blocking

## Adding a New Driver

1. Create `src/drivers/example/<name>driver.c`
2. Implement `struct kdal_driver_ops` (at minimum: `probe`, `remove`, `read`, `write`)
3. Add `init()` / `exit()` functions
4. Wire into `src/core/kdal.c` init cascade
5. Add to `Makefile` (`kdal-y += ...`)
6. Add KUnit tests in `tests/kunit/`
7. Update `tools/kdality/kdalctl.c` device class name mapping

## Adding a New Backend

See the [Porting Guide](/kernel/porting/).

## Adding a `.kdh` Standard Library Device

1. Create `lang/stdlib/<device>.kdh`
2. Define `device`, `register_map`, `signals`, `capabilities`, `power_states`
3. Follow existing patterns - see `lang/stdlib/uart.kdh` as reference
4. Create a matching example `.kdc` file in `examples/`
5. Add a test case in `tests/userspace/` or extend `test.sh`

## Modifying the Compiler

The compiler lives in `compiler/` and produces `libkdalc.a` + `kdalc`:

```
compiler/
├── lexer.c       # Hand-written tokenizer (80+ token types)
├── parser.c      # Recursive descent → arena-allocated AST
├── sema.c        # Symbol resolution, register validation
├── codegen.c     # C code generation + Kbuild emitter
├── main.c        # Standalone kdalc entry point
├── CMakeLists.txt # Builds libkdalc.a and kdalc
└── include/      # token.h, ast.h, codegen.h
```

Rules:
- All public API uses `kdal_` prefix (e.g. `kdal_lex()`, `kdal_parse()`)
- AST nodes are arena-allocated - never free individual nodes
- Codegen must emit kernel-style C - tabs, 80-col, C89 comments
- New token types → update `token.h` enum + lexer keyword table
- New AST nodes → update `ast.h` union + parser + sema + codegen

## Testing Requirements

- All new kernel code must have KUnit tests
- All new ioctl paths must have userspace test coverage
- Integration tests for multi-device interactions
- Benchmark measurements for performance-sensitive changes

## License

By contributing, you agree that your contributions will be licensed under
GPL-3.0-or-later.
