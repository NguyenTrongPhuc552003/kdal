# KDAL - Kernel Device Abstraction Layer

**A groundbreaking, revolutionary project in the embedded software industry**  
*Exactly like Linus Torvalds did with Linux as a personal hobby.*

## Project Overview

**Repository name:** `kdal`

**Full description:**  
Kernel Device Abstraction Layer — a hardware-agnostic, pluggable abstraction framework inside the Linux kernel that lets developers write portable device drivers and accelerator backends **once** and run them on **any** embedded platform.

- **Development starts on QEMU aarch64** (Mac M4 Pro host → aarch64 guest)  
- **Later target:** Radxa Orion O6 with CIX P1 SoC, Arm Immortalis G720 GPU, and 30 TOPS NPU

## The Problem Today and in the Future

- **Today:** Device drivers are tightly coupled to specific SoCs, kernel versions, and vendor BSPs. Porting across platforms or kernel updates is manual, error-prone, and creates massive code duplication. Debugging, power management, and accelerator integration (GPU/NPU) remain fragmented.
- **Future:** Edge AI, heterogeneous computing, and the explosion of new SoC releases (Armv9+, NPUs everywhere) will make this problem 10× worse without a unified layer. This fragmentation slows innovation, raises costs, and prevents the “Linux moment” for embedded drivers and accelerators.

## The Groundbreaking Solution

**KDAL** is a thin, efficient kernel abstraction layer (inspired by virtio but designed for both real and emulated hardware) that delivers:

- Clean driver API (`register`/`unregister`, `read`/`write`/`ioctl`, power states, events)
- Pluggable hardware backends (QEMU virtio first, then real SoC-specific)
- Built-in support for classic peripherals (UART, I2C, SPI, GPIO) **and** accelerator abstraction (GPU/NPU command queues, memory sharing)
- Out-of-tree kernel module + user-space tools
- Full QEMU-based development and testing on Mac M4 Pro (aarch64)
- **Goal:** 70–90 % reduction in porting effort, automatic compatibility across kernels, and easier upstreaming

## Repository Structure

```bash
kdal
├── .github
│   ├── workflows
│   │   ├── build.yml
│   │   ├── test.yml
│   │   ├── ci.yml
│   │   ├── codeql.yml
│   │   └── release.yml
│   └── templates
│       ├── bug_report.md
│       ├── feature_request.md
│       ├── pull_request.md
│       └── kernel_port.md
├── .gitignore
├── .clang-format
├── .editorconfig
├── .gitattributes
├── CODE_OF_CONDUCT.md
├── CONTRIBUTING.md
├── LICENSE
├── README.md
├── SECURITY.md
├── CHANGELOG.md
├── CITATION.cff
├── Makefile
├── Kconfig
├── CMakeLists.txt
├── include
│   └── kdal
│       ├── core
│       │   ├── kdal.h
│       │   └── version.h
│       ├── api
│       │   ├── driver.h
│       │   ├── accel.h
│       │   └── common.h
│       ├── backend.h
│       ├── types.h
│       └── ioctl.h
├── src
│   ├── core
│   │   ├── kdal.c
│   │   ├── registry.c
│   │   ├── event.c
│   │   ├── module.c
│   │   ├── chardev.c
│   │   ├── debugfs.c
│   │   └── power.c
│   ├── drivers
│   │   └── example
│   │       ├── uartdriver.c
│   │       ├── gpudriver.c
│   │       ├── i2cdriver.c
│   │       └── spidriver.c
│   └── backends
│       ├── qemu
│       │   ├── qemubackend.c
│       │   ├── virtioaccel.c
│       │   ├── virtioio.c
│       │   └── mmio.c
│       └── generic
│           ├── platdev.c
│           ├── socglue.c
│           └── devicetree.c
├── tests
│   ├── kunit
│   │   ├── test_core.c
│   │   ├── test_registry.c
│   │   ├── test_event.c
│   │   ├── test_driver.c
│   │   └── test_accel.c
│   ├── integration
│   │   └── kdaltest.c
│   └── userspace
│       ├── testapp.c
│       └── benchmark.c
├── docs
│   ├── architecture.md
│   ├── portingguide.md
│   ├── installation.md
│   ├── language_guide.md
│   ├── api_reference.md
│   ├── faq.md
│   ├── performance.md
│   ├── upstreaming.md
│   └── thesis.md
├── scripts
│   ├── setupqemu
│   │   ├── run.sh
│   │   ├── buildkernel.sh
│   │   ├── prepare.sh
│   │   └── debug.sh
│   ├── ci
│   │   ├── test.sh
│   │   ├── checkpatch.sh
│   │   ├── static_analysis.sh
│   │   └── coverage.sh
│   ├── devsetup
│   │   └── install_deps.sh
│   └── release
│       ├── bumpversion.sh
│       └── generate_changelog.sh
├── tools
│   └── kdality
│       ├── main.c
│       ├── Makefile
│       ├── kdalctl.c
│       ├── compile.c
│       ├── init.c
│       ├── dtgen.c
│       ├── simulate.c
│       ├── testgen.c
│       ├── lint.c
│       └── templates.h
├── compiler
│   ├── CMakeLists.txt
│   ├── Makefile
│   ├── main.c
│   ├── lexer.c
│   ├── parser.c
│   ├── sema.c
│   ├── codegen.c
│   └── include
│       ├── ast.h
│       ├── token.h
│       └── codegen.h
├── lang
│   ├── spec.md
│   ├── DESIGN.md
│   ├── grammar
│   │   ├── kdh.ebnf
│   │   └── kdc.ebnf
│   └── stdlib
│       ├── common.kdh
│       ├── uart.kdh
│       ├── i2c.kdh
│       ├── spi.kdh
│       ├── gpio.kdh
│       ├── gpu.kdh
│       └── npu.kdh
├── editor
│   └── vscode
│       └── kdal-lang
│           ├── package.json
│           ├── language-configuration.json
│           ├── README.md
│           ├── syntaxes
│           │   ├── kdh.tmLanguage.json
│           │   └── kdc.tmLanguage.json
│           └── snippets
│               ├── kdh.json
│               └── kdc.json
├── examples
│   ├── kdc_hello
│   │   ├── README.md
│   │   ├── uart_hello.kdh
│   │   ├── uart_hello.kdc
│   │   └── i2c_sensor.kdc
│   ├── minimaldriver
│   │   ├── driver.c
│   │   └── Makefile
│   └── accel_demo
│       ├── gpudemo.c
│       └── npudemo.c
├── packaging
│   ├── debian
│   │   ├── control
│   │   ├── changelog
│   │   └── rules
│   └── rpm
│       └── kdal.spec
└── cmake
    ├── FindKDAL.cmake
    ├── KDALConfig.cmake
    ├── KDALConfigVersion.cmake
    ├── KDALPackaging.cmake
    ├── KDALTargets.cmake
    ├── KDALTesting.cmake
    ├── KDALToolchain.cmake
    └── CompileKDC.cmake
```

## Global Requirements

- All code written in C following Linux kernel coding style (`checkpatch` clean)
- Full support for QEMU aarch64 `virt` machine on Mac M4 Pro (aarch64 guest)
- Primary build system: Makefile + kbuild; CMake supported as optional
- Comprehensive comments and documentation in every file
- Designed for easy upstreaming (clean, well-documented, GPL-compliant)

## Implementation Status

### Fully Implemented

| Component         | Status       | Key Files                                                   |
| ----------------- | ------------ | ----------------------------------------------------------- |
| Core runtime      | **Complete** | `kdal.c`, `registry.c`, `event.c`, `chardev.c`, `debugfs.c` |
| UART driver       | **Complete** | `uartdriver.c` — baud/parity/stop config, ring buffer I/O   |
| I2C driver        | **Complete** | `i2cdriver.c` — bus speed, slave addr, 256B buffer          |
| SPI driver        | **Complete** | `spidriver.c` — clock/mode/bpw, 4096B buffer                |
| GPU driver        | **Complete** | `gpudriver.c` — full accel ops (queue/buffer/submit)        |
| QEMU backend      | **Complete** | `qemubackend.c` — 4096B per-device ring buffer              |
| Virtio stubs      | **Complete** | `virtioio.c`, `virtioaccel.c` — readiness API               |
| Generic backends  | **Complete** | `platdev.c`, `socglue.c`, `devicetree.c`                    |
| Userspace tool    | **Complete** | `kdalctl.c` — version/list/info/power/read/write CLI        |
| Compiler (kdalc)  | **Alpha**    | lexer.c, parser.c, sema.c, codegen.c — .kdc → C transpiler  |
| kdality toolchain | **Complete** | compile, init, dt-gen, simulate, test-gen, lint subcommands |
| KDAL language     | **Draft**    | spec.md, DESIGN.md, 2 EBNF grammars, 7 stdlib .kdh files    |
| VS Code extension | **Complete** | TextMate grammars + 14 snippets for .kdh/.kdc               |
| KUnit tests       | **Complete** | 38 test cases across 5 suites                               |
| Integration tests | **Complete** | `testapp.c`, `benchmark.c`, `kdaltest.c`                    |
| Scripts           | **Complete** | QEMU setup, CI, release, dependency management              |
| CI workflows      | **Complete** | Build, test, lint, CodeQL, release                          |
| Documentation     | **Complete** | 8 docs + README + CONTRIBUTING + CHANGELOG                  |
| Examples          | **Complete** | Minimal driver, GPU demo, NPU demo                          |

### Planned (Next Phase)

- GPIO driver class
- NPU driver for CIX P1 (Radxa Orion O6)
- Real virtio transport implementation
- DMA zero-copy backend
- Performance measurements on real hardware

---

**Master thesis — Computer Engineering**
