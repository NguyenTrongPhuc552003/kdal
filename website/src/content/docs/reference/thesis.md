---
title: Thesis Context
description: How KDAL maps to the master's thesis chapters and evaluation criteria.
---

## Title

**KDAL: A Kernel Device Abstraction Layer for Portable Embedded Driver
and Accelerator Development**

## Abstract

Device driver development in embedded Linux remains tightly coupled to specific
SoCs, kernel versions, and vendor BSPs. This thesis presents KDAL, a thin
kernel abstraction layer that separates driver logic from hardware transport.
The framework is validated on QEMU aarch64 with four device classes and
benchmarked for abstraction overhead. Results demonstrate that a well-designed
abstraction reduces porting effort by 70–90% while adding negligible runtime
cost.

## Chapter Structure

### Chapter 1 - Introduction & Problem Statement

**Thesis question:** Can a thin kernel abstraction layer significantly reduce
the effort of porting device drivers across embedded platforms without
measurable performance degradation?

**Problem framing:**
- Driver code is 70%+ of the Linux kernel by LOC
- Vendor BSPs duplicate driver logic for each SoC variant
- Porting a single driver across platforms costs weeks of engineering time
- GPU/NPU accelerator integration is even more fragmented

### Chapter 2 - Background & Related Work

| Approach     | Coverage                       | Limitation            |
| ------------ | ------------------------------ | --------------------- |
| Linux virtio | Virtualised I/O                | Not for real hardware |
| Zephyr HAL   | Microcontrollers               | Different kernel      |
| Android HAL  | Mobile devices                 | Userspace-only, Java  |
| Vendor BSPs  | Specific SoC                   | No portability        |
| **KDAL**     | **Peripherals + accelerators** | **Thesis scope**      |

### Chapter 3 - Architecture & Design

Key contribution: the six-layer architecture with clean contracts at each
boundary.

| Decision              | Rationale                        | Alternative             |
| --------------------- | -------------------------------- | ----------------------- |
| Single misc device    | Simplicity, dynamic selection    | Per-device char devices |
| ioctl control plane   | Standard Linux pattern           | sysfs attributes        |
| Ring buffer for QEMU  | Zero hardware dependency         | Shared memory region    |
| Mutex over spinlock   | Sleep-capable, correctness first | RCU for read path       |
| Circular event buffer | Bounded memory, O(1)             | Linked list with limits |

### Chapter 4 - Implementation

| Component        | LOC (approx.) | Key file                                   |
| ---------------- | ------------- | ------------------------------------------ |
| Core runtime     | ~600          | `src/core/kdal.c`, `registry.c`, `event.c` |
| Character device | ~250          | `src/core/chardev.c`                       |
| Debug interface  | ~120          | `src/core/debugfs.c`                       |
| QEMU backend     | ~200          | `src/backends/qemu/qemubackend.c`          |
| UART driver      | ~200          | `src/drivers/example/uartdriver.c`         |
| I2C driver       | ~200          | `src/drivers/example/i2cdriver.c`          |
| SPI driver       | ~200          | `src/drivers/example/spidriver.c`          |
| GPU driver       | ~300          | `src/drivers/example/gpudriver.c`          |
| Userspace tool   | ~300          | `tools/kdality/kdalctl.c`                  |

### Chapter 5 - Testing & Validation

| Test type         | Count    | Purpose                          |
| ----------------- | -------- | -------------------------------- |
| KUnit (in-kernel) | 38 cases | Internal API correctness         |
| Userspace smoke   | 1 suite  | End-to-end ioctl validation      |
| Integration       | 1 suite  | Multi-device, power, error paths |
| Benchmark         | 1 tool   | Throughput measurement           |

### Chapter 6 - Results & Evaluation

Metrics to report:

1. **Porting effort** - LOC changed to add a new backend (target: < 200 LOC)
2. **Runtime overhead** - Latency delta vs. direct access (target: < 5%)
3. **Throughput** - Write/read per device class
4. **Code reuse** - Driver code shared across backends (target: > 85%)
5. **Test coverage** - Line and branch coverage from `gcov`

### Chapter 7 - Discussion & Future Work

- Abstraction leakage risks
- Scalability to NPU and multi-queue accelerators
- Real-time constraints and PREEMPT_RT compatibility
- Comparison with virtio transport layer
- Port to Radxa Orion O6 (CIX P1 SoC)

### Chapter 8 - Conclusion

Restate thesis question → summarise findings → highlight contributions.

## Repository-to-Thesis Mapping

| Thesis section    | Repository artifact                                  |
| ----------------- | ---------------------------------------------------- |
| Problem statement | `.github/project/DESCRIPTION.md`                     |
| Architecture      | `docs/architecture.md`, `include/kdal/`              |
| Implementation    | `src/`, `Makefile`                                   |
| Testing           | `tests/`, `scripts/ci/test.sh`                       |
| Results           | `tests/userspace/benchmark.c`, `docs/performance.md` |
| Reproducibility   | `scripts/env/`, `docs/installation.md`               |
| Porting           | `docs/portingguide.md`, `src/backends/`              |
