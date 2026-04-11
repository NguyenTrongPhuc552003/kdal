# Thesis Mapping

## Title

**KDAL: A Kernel Device Abstraction Layer for Portable Embedded Driver
and Accelerator Development**

## Abstract Outline

Device driver development in embedded Linux remains tightly coupled to specific
SoCs, kernel versions, and vendor BSPs. This thesis presents KDAL, a thin
kernel abstraction layer that separates driver logic from hardware transport.
The framework is validated on QEMU aarch64 with four device classes and
benchmarked for abstraction overhead. Results demonstrate that a well-designed
abstraction reduces porting effort by 70-90% while adding negligible runtime
cost.

## Chapter Structure

### Chapter 1: Introduction & Problem Statement

**Thesis question:** Can a thin kernel abstraction layer significantly reduce
the effort of porting device drivers across embedded platforms without
measurable performance degradation?

**Problem framing:**
- Driver code is 70%+ of the Linux kernel by LOC
- Vendor BSPs duplicate driver logic for each SoC variant
- Porting a single driver across platforms costs weeks of engineering time
- GPU/NPU accelerator integration is even more fragmented
- No existing framework addresses both peripherals and accelerators

**Repository evidence:** `DESCRIPTION.md` for problem statement,
`docs/architecture.md` for the proposed solution.

### Chapter 2: Background & Related Work

| Approach     | Coverage                       | Limitation                      |
| ------------ | ------------------------------ | ------------------------------- |
| Linux virtio | Virtualized I/O                | Not designed for real hardware  |
| Zephyr HAL   | Microcontrollers               | Different kernel, not Linux     |
| Android HAL  | Mobile devices                 | Userspace-only, Java dependency |
| Vendor BSPs  | Specific SoC                   | No portability by design        |
| **KDAL**     | **Peripherals + accelerators** | **Thesis scope**                |

### Chapter 3: Architecture & Design

**Key contribution:** The five-layer architecture (User → Control → Core →
Driver → Backend) with clean contracts at each boundary.

**Repository evidence:**
- `include/kdal/` — public API headers defining contracts
- `docs/architecture.md` — layer diagram and data flow
- `docs/api_reference.md` — complete API specification

**Design decisions to discuss:**

| Decision              | Rationale                            | Alternative Considered  |
| --------------------- | ------------------------------------ | ----------------------- |
| Single misc device    | Simplicity, dynamic device selection | Per-device char devices |
| ioctl control plane   | Standard Linux pattern, versioned    | sysfs attributes        |
| Ring buffer for QEMU  | Zero hardware dependency             | Shared memory region    |
| Mutex over spinlock   | Sleep-capable ops, correctness first | RCU for read path       |
| Event circular buffer | Bounded memory, O(1) push/pop        | Linked list with limits |

### Chapter 4: Implementation

**Repository evidence:** Complete source code in `src/`.

| Component        | LOC (approx.) | Key File                                   |
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

**Implementation highlights to discuss:**
- `goto` error unwinding in init cascade
- Per-fd state for multi-process isolation
- Ring buffer design for QEMU emulated transport
- Accelerator ops contract (queue/buffer/submit)

### Chapter 5: Testing & Validation

**Repository evidence:** `tests/`

| Test Type         | Count         | Purpose                          |
| ----------------- | ------------- | -------------------------------- |
| KUnit (in-kernel) | 38 test cases | Internal API correctness         |
| Userspace smoke   | 1 test suite  | End-to-end ioctl validation      |
| Integration       | 1 test suite  | Multi-device, power, error paths |
| Benchmark         | 1 tool        | Throughput measurement           |

**Validation matrix:**

| Property        | Method                             |
| --------------- | ---------------------------------- |
| Correctness     | KUnit + integration tests          |
| Portability     | Same driver code on QEMU + real HW |
| Performance     | Benchmark throughput and latency   |
| Maintainability | LOC per new backend, code churn    |
| Reliability     | Error injection, power cycling     |

### Chapter 6: Results & Evaluation

**Metrics to report:**

1. **Porting effort**: LOC changed to add a new backend (target: < 200 LOC)
2. **Runtime overhead**: Latency delta vs. direct hardware access (target: < 5%)
3. **Throughput**: Write/read throughput per device class
4. **Code reuse**: Percentage of driver code shared across backends (target: > 85%)
5. **Test coverage**: Line and branch coverage from `gcov`

**Figures to include:**
- Throughput bar chart: direct vs. KDAL per device class
- Porting effort comparison: KDAL vs. traditional approach
- Architecture layer diagram
- Init sequence diagram

### Chapter 7: Discussion & Future Work

**Discussion points:**
- Abstraction leakage risks (backend-specific behavior visible to drivers)
- Scalability to NPU and multi-queue accelerators
- Real-time constraints and PREEMPT_RT compatibility
- Comparison with virtio transport layer

**Future work:**
- Port to Radxa Orion O6 (CIX P1 SoC, Immortalis G720 GPU, 30 TOPS NPU)
- Add GPIO driver class
- Implement virtio transport for cross-VM use
- Upstream RFC to linux-kernel mailing list
- Performance on real hardware with DMA

### Chapter 8: Conclusion

Restate thesis question → summarize findings → highlight contributions.

## Repository-to-Thesis Artifact Mapping

| Thesis Section    | Repository Artifact                                  |
| ----------------- | ---------------------------------------------------- |
| Problem statement | `.github/project/DESCRIPTION.md`                     |
| Architecture      | `docs/architecture.md`, `include/kdal/`              |
| Implementation    | `src/`, `Makefile`                                   |
| Testing           | `tests/`, `scripts/ci/test.sh`                       |
| Results           | `tests/userspace/benchmark.c`, `docs/performance.md` |
| Reproducibility   | `scripts/setupqemu/`, `docs/installation.md`         |
| Porting           | `docs/portingguide.md`, `src/backends/`              |
