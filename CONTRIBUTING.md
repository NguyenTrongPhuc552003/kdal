# Contributing to KDAL

KDAL is a master thesis project with production-grade engineering standards.
Contributions must preserve API clarity, QEMU-first reproducibility, and
Linux kernel coding discipline.

## Prerequisites

| Tool       | Minimum Version | Check                  |
| ---------- | --------------- | ---------------------- |
| GCC        | 11+             | `gcc --version`        |
| CMake      | 3.20+           | `cmake --version`      |
| Node.js    | 18+             | `node --version`       |
| npm        | 8+              | `npm --version`        |
| ShellCheck | any             | `shellcheck --version` |
| cppcheck   | any             | `cppcheck --version`   |

> **Tip:** Run `./scripts/env/dependencies.sh` to install everything automatically.

## Development Setup

```sh
# Install build tools
./scripts/env/dependencies.sh

# Build everything (compiler + tool)
./scripts/dev/build.sh --variant debug all

# Prepare the reproducible QEMU workspace when you need guest-side testing
./scripts/env/prepare.sh
./scripts/env/kernel.sh

# Run CI suite
./scripts/ci/test.sh

# Run static analysis
./scripts/ci/static_analysis.sh

# Run style check
./scripts/ci/checkpatch.sh
```

## Code Style

- **Kernel code**: Linux kernel coding style (`checkpatch.pl` clean)
  - Tabs for indentation
  - 80-column soft limit
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
4. Run `./scripts/ci/checkpatch.sh` on changed files
5. Submit PR with description of changes and testing done

## Architecture Rules

- **No backend-specific types in public headers** (`include/kdal/`)
- **No direct hardware access in core or drivers** - always through backend ops
- **No new ioctl without version bump** - see `scripts/release/bump_version.sh`
- **No `printk` spam** - use `pr_debug()` for development, `pr_info()` for lifecycle events
- **No sleeping in registry iteration** - callbacks must be non-blocking

## Adding a New Driver

1. Create `src/drivers/example/<name>driver.c`
2. Implement `struct kdal_driver_ops` (at minimum: `probe`, `remove`, `read`, `write`)
3. Add `init()`/`exit()` functions
4. Wire into `src/core/kdal.c` init cascade
5. Add to `Makefile` (`kdal-y += ...`)
6. Add KUnit tests in `tests/kunit/`
7. Update `tools/kdality/kdalctl.c` device class name mapping

## Adding a New Backend

See [docs/portingguide.md](docs/portingguide.md).

## Adding a `.kdh` Standard Library Device

1. Create `lang/stdlib/<device>.kdh`
2. Define `device`, `register_map`, `signals`, `capabilities`, `power_states`
3. Follow existing patterns - see `lang/stdlib/uart.kdh` as reference
4. Create a matching example `.kdc` file in `examples/`
5. Add a test case in `tests/userspace/` or extend `test.sh`
6. Update `docs/language_guide.md` device reference table

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
- **All public API uses `kdal_` prefix** (e.g. `kdal_lex()`, `kdal_parse()`)
- **AST nodes are arena-allocated** - never free individual nodes
- **Codegen must emit kernel-style C** - tabs, 80-col, `/* C89 comments */`
- **New token types** → update `token.h` enum + lexer keyword table
- **New AST nodes** → update `ast.h` union + parser + sema + codegen
- Build with `./scripts/dev/build.sh --variant debug kdalc` and test with `./build/debug/compiler/kdalc <file>.kdc -o /tmp/out`

## Testing Requirements

- All new kernel code must have KUnit tests
- All new ioctl paths must have userspace test coverage
- Integration tests for multi-device interactions
- Benchmark measurements for performance-sensitive changes

## License

By contributing, you agree that your contributions will be licensed under GPLv3.
