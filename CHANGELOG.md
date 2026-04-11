# Changelog

All notable changes to this project are documented in this file.  
Format follows [Keep a Changelog](https://keepachangelog.com/).

## [0.1.0] — 2025-06-09

### Added

#### Toolchain Subcommands
- `kdality init <name>` — scaffold new driver projects (.kdh + .kdc + Makefile + README)
- `kdality dt-gen <file.kdh>` — generate Device Tree Source overlays from .kdh headers
- `kdality simulate <file.kdc>` — dry-run interpreter with register trace (no kernel needed)
- `kdality test-gen <file.kdc>` — auto-generate KUnit test stubs from driver handlers
- `kdality lint <file>` — static analysis with 9 rules (W001–W009) and --strict mode

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
- Architecture: 5-layer diagram, data flow, init cascade, design decisions
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

#### Build System
- Created `compiler/CMakeLists.txt` — builds `kdalc_lib` static library and `kdalc` binary
- Fixed top-level `CMakeLists.txt` — added all 8 kdality source files, linked kdalc_lib, removed broken `include(KDALConfig)`
- Fixed kdality CMake target include paths for both `include/` and `compiler/include/`

#### Stale References
- Renamed all `kdaltool` references to `kdality` across CI workflows, docs, and source
- Fixed stale `scripts/qemu/run.sh` path to `scripts/setupqemu/run.sh`
- Replaced placeholder `blink.kdc`/`sensor.kdc` example paths with actual `uart_hello.kdc`/`i2c_sensor.kdc`
- Updated `your-org/kdal` and `example/kdal` placeholder URLs to actual GitHub repository
