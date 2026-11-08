# Changelog

All notable changes to this project are documented in this file.  
Format follows [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

Changes since v0.1.0. The language, compiler, editor plugins and tools
(`lang/`, `compiler/`, `editor/`, `tools/`) are frozen since 2026-11-01.

### Added

- Devices report their resource demand (MMIO range, GIC INTIDs, SMMU and
  StreamIDs) in `struct kdal_demand`; `/sys/kernel/debug/kdal/demand` prints
  it as JSON with the FF-A partition manifest property names. The
  device-tree backend fills it from `reg`, `interrupts` and `iommus`
  (audit K4).
- KUnit suite `kdal-demand` (2 cases).
- `make KDAL_KUNIT=y` builds the KUnit suites into `kdal.ko` (audit K12).
- CI on GitHub Actions: smoke tests on Linux and macOS, lint, and a job that
  builds `kdal.ko` against Linux v6.18 arm64, runs its KUnit suites in QEMU
  and checks the reported device demand (audit K1).
- Local CodeQL runner (`scripts/dev/codeql.sh`).
- Issue templates and a pull request template.

### Changed

- The KUnit test `find_driver_by_class` checks the driver class, not which
  driver registers first (audit K11).
- The project home moved to Codeberg on 2026-11-01 and back to GitHub on
  2026-11-04; installer, release scripts, packaging and links point to
  GitHub again.
- README and docs: KDAL's scope in the HAVEN thesis, the CIX P1 target
  platforms, the four drivers named example drivers, the KUnit suite table,
  and which directories are maintained or frozen.
- Compiler and `kdality` refactors: parser statement handling split, compile
  option parsing tightened, simulate expression dispatch, testgen parsing and
  token enum numbering simplified.
- Em-dashes replaced with ASCII hyphens; local CodeQL artifacts ignored.

### Fixed

- The module builds against Linux v6.18 with clang (`file_operations` and
  accelerator op signatures) and loads without a crash: a driver is bound to
  its device before probe. Out-of-tree builds default
  `CONFIG_KDAL_QEMU_BACKEND` and `CONFIG_KDAL_EXAMPLE_DRIVERS` to y, as
  Kconfig does (audit K7, K8, K9).
- Registering an object that is already registered no longer resets its list
  links and hangs the registry (audit K10).
- `kdal_core_init` runs the generic backend (SoC glue, platform backend,
  device-tree scan), so `kdal,device` nodes are found (audit K15).
- Compiler and `kdality`: input and output paths are validated (CodeQL
  path-expression alerts), files are created with restricted permissions,
  and `dt-gen` and `init` file handling is hardened.
- CI: `kernel-prepare.sh` accepts a relative work directory; QEMU runs
  without a network card.

## [0.1.0] - 2025-06-09

### Added

#### Build System - CMake Migration
- Migrated from GNU Make to **CMake 3.20+** as the unified build system
- Added convenience scripts: `build.sh`, `run.sh`, `test.sh` under `scripts/dev/`
- Build output organization: all artifacts collected under `build/` with subfolders
- `build.sh` `collect_outputs()` copies website dist → `build/website/`, vsix → `build/editor/vscode/`
- CMake `uninstall` target via `install_manifest.txt`
- Vim/Neovim plugin install rule for editor/vim/vim-kdal/ subdirectories

#### Development Test Script
- `test.sh` with numbered `[N/M]` PASS/FAIL test cases across 10 targets
- CI-mirroring tests: `cppcheck`, `shellcheck`, `structure` validation
- Test report logging: saves results to `build/test/<target>_<YYYYMMDD_HHMMSS>.log`

#### Compiler Improvements
- Block-based arena allocator (`kdal_arena_t`) - linked-list of blocks, no `realloc`
- Dotted name support in `driver ... for DEV.Name` clause (qualified identifiers)
- Backend block no longer requires trailing semicolon (consistent with driver/device blocks)

#### Toolchain Subcommands
- `kdality init <name>` - scaffold new driver projects (.kdh + .kdc + Makefile + README)
- `kdality dt-gen <file.kdh>` - generate Device Tree Source overlays from .kdh headers
- `kdality simulate <file.kdc>` - dry-run interpreter with register trace (no kernel needed)
- `kdality test-gen <file.kdc>` - auto-generate KUnit test stubs from driver handlers
- `kdality lint <file>` - static analysis with 9 rules (W001–W009) and --strict mode

#### VS Code Extension
- TextMate grammars for `.kdh` and `.kdc` syntax highlighting
- Code snippets: device skeleton, driver skeleton, handler stubs, register access
- Language configuration with comment toggling and bracket matching

#### Core Runtime
- Module init/exit with full cascade (chardev → debugfs → backends → drivers)
- Registry with linked-list storage for backends, drivers, and devices
- Duplicate name prevention and find-by-name/class queries
- Device iterator (`kdal_for_each_device()`)
- Registry snapshot counters
- Circular event buffer (256 entries) with mutex and waitqueue
- Per-device power state management with event emission

#### Control Plane
- Character device `/dev/kdal` via `misc_register()`
- Per-fd state with `kdal_file_ctx` in `file->private_data`
- 6 ioctl commands: GET_VERSION, GET_INFO, SET_POWER, LIST_DEVICES, POLL_EVENT, SELECT_DEV
- Read/write dispatch to selected device's driver
- debugfs interface at `/sys/kernel/debug/kdal/` with devices, version, snapshot files

#### Drivers
- UART driver with ring buffer allocation, baud/parity/stop config
- I2C driver with bus speed and slave address config
- SPI driver with clock/mode/bits-per-word config
- GPU driver with accelerator ops (queue/buffer/submit)

#### Backends
- QEMU backend with 4096-byte per-device ring buffer
- Virtio I/O readiness API stub
- Virtio accelerator readiness API stub
- MMIO helper with 8/16/32-bit read/write
- Generic platform device backend
- SoC glue layer stub for CIX P1
- Device Tree scanner (`CONFIG_OF` guarded)

#### Userspace
- `kdality` CLI: version, list, info, power, read, write commands
- Smoke test (`testapp`): GET_VERSION, LIST_DEVICES, GET_INFO, loopback
- Integration test (`kdaltest`): multi-device, power cycling, error paths
- Throughput benchmark with configurable parameters

#### Testing
- 38 KUnit test cases across 5 suites (core, registry, event, driver, accel)
- CI test script with 4-stage pipeline

#### Infrastructure
- QEMU workspace preparation (cloud image, cloud-init, 9p shared dir)
- Kernel cross-compilation script for Linux 6.6.80 aarch64
- QEMU launch and debug scripts
- `checkpatch.pl` wrapper
- Static analysis (cppcheck, sparse, shellcheck)
- Coverage collection (gcov/lcov)
- Version bump script
- Changelog generator from conventional commits
- Dependency installer for macOS and Linux
- GitHub Actions CI workflows (build, test, lint, CodeQL, release)

#### Documentation
- Architecture: 6-layer diagram, data flow, init cascade, design decisions
- API Reference: all functions, ioctl table, types, contracts
- Installation: macOS/Linux setup, build, verification, troubleshooting
- Porting Guide: step-by-step backend implementation
- Performance: measurement methodology, planned metrics
- FAQ: 12 common questions
- Upstreaming: strategy, checklist, patch series plan
- Thesis: chapter mapping to repository artifacts

#### Language Ecosystem
- KDAL DSL with `.kdh` device headers and `.kdc` driver files
- Compiler pipeline: lexer → parser → AST → semantic analysis → C codegen
- 7 standard library `.kdh` files (UART, I2C, SPI, GPIO, GPU, NPU, generic)
- 8 CMake modules for IDE integration and cross-compilation
- `kdality compile` subcommand linking libkdalc.a
- Standalone `kdalc` compiler binary
- Language specification (`lang/spec.md`) and design rationale (`lang/DESIGN.md`)
- VS Code grammar stubs for `.kdh` and `.kdc` syntax highlighting

#### Examples
- Minimal third-party driver (out-of-tree kernel module template)
- GPU accelerator demo (userspace)
- NPU accelerator demo (userspace)
- KDAL DSL driver examples (`examples/kdc_hello/uart_hello.kdc`, `examples/kdc_hello/i2c_sensor.kdc`)

### Fixed

#### Compiler - Critical Parser Bug
- Fixed buffer overflow in `kdal_ast_new()` - LET_STMT and FOR_STMT now allocate `sizeof(kdal_expr_node_t)` (72 bytes) instead of `sizeof(kdal_ast_t)` (32 bytes)
- Refactored arena allocator from single-buffer `realloc` to block-based linked list (pointers remain stable across allocations)
- Removed incorrect trailing semicolon requirement after backend `{ }` block
- Added dotted identifier consumption for `driver X for DEV.Name` syntax

#### Static Analysis (cppcheck)
- Fixed `constParameter` warnings in 6 kdality subcommand files (`argv[]` → `*const argv[]`)
- Fixed `constVariablePointer` warnings for `strchr()` return values in dtgen.c, lint.c, simulate.c
- Fixed `scanf` format specifier (`%li` → `%lu`) for `unsigned long` in simulate.c
- Fixed `variableScope` warnings by moving declarations into narrower scopes
- Fixed `constParameterPointer` in benchmark.c time_diff_ms function

#### Build System
- Created `compiler/CMakeLists.txt` - builds `kdalc_lib` static library and `kdalc` binary
- Fixed top-level `CMakeLists.txt` - added all 8 kdality source files, linked kdalc_lib, removed broken `include(KDALConfig)`
- Fixed kdality CMake target include paths for both `include/` and `compiler/include/`

#### E2E Test Fixes
- Fixed `test_user_journey.sh` - corrected kdality binary path to `$BUILD_DIR/kdality`
- Fixed `test_user_journey.sh` - replaced `make uninstall` with `install_manifest.txt`-based uninstall
- Fixed CMake module count assertion (1 → 7)
- Added shellcheck directive suppressions across 8 E2E scripts
- Added trailing newlines to 8 C source files

#### Stale References
- Renamed all `kdaltool` references to `kdality` across CI workflows, docs, and source
- Fixed stale `scripts/qemu/run.sh` path to `scripts/setupqemu/run.sh`
- Replaced placeholder `blink.kdc`/`sensor.kdc` example paths with actual `uart_hello.kdc`/`i2c_sensor.kdc`
- Updated `your-org/kdal` and `example/kdal` placeholder URLs to actual GitHub repository

### Notes

- **Tested on**: macOS aarch64 (host build + toolchain), QEMU aarch64 virt machine (kernel module)
- **No real hardware validation** - SoC glue layer is `CONFIG_OF`-guarded and a no-op on QEMU
- **QEMU/Docker VS Code integration** deferred to v0.2.0
