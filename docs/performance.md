# Performance

> **Status (v0.1.0-alpha):** Methodology and tooling defined. Baseline
> measurements pending - requires QEMU guest with the KDAL module loaded.
> Results will be populated once the runtime is tested on the target platform.

## Overview

KDAL adds a thin abstraction layer between userspace and hardware. The
performance cost is one indirect function call per I/O operation through the
driver ops table, plus a mutex acquisition in the registry for device lookup.
This section documents the measurement methodology and baseline results.

## Measurement Methodology

### Benchmark Tool

`tests/userspace/benchmark.c` measures write/read throughput through
`/dev/kdal` using configurable parameters:

| Parameter  | Default  | Description                 |
| ---------- | -------- | --------------------------- |
| Device     | `uart0`  | Target device name          |
| Iterations | 10000    | Number of write/read cycles |
| Block size | 64 bytes | Payload per operation       |

Usage:
```sh
./benchmark              # defaults
./benchmark spi0 50000 256  # SPI, 50K iterations, 256-byte blocks
```

Timing uses `clock_gettime(CLOCK_MONOTONIC)` for nanosecond resolution.

### Planned Metrics

| Metric                | Method                             | Target                    |
| --------------------- | ---------------------------------- | ------------------------- |
| Registration overhead | Time `kdal_register_*()` in KUnit  | < 10 µs                   |
| Control-path latency  | Time single ioctl round-trip       | < 50 µs                   |
| Data-path throughput  | Sustained write/read via benchmark | > 10 MB/s (QEMU ring)     |
| Backend I/O latency   | Time single read/write op          | < 5 µs (QEMU ring)        |
| Code churn            | LOC diff between backends          | < 15% of total            |
| Porting effort        | New backend implementation time    | < 1 day for simple device |

### Collection

#### QEMU (Emulated)

On QEMU, the ring buffer runs entirely in software. Results measure
abstraction overhead without hardware latency:

```sh
# Inside QEMU guest
insmod /mnt/shared/kdal.ko
./benchmark uart0 100000 64
./benchmark i2c0 100000 64
./benchmark spi0 100000 64
./benchmark gpu0 100000 64
```

#### Real Hardware (Radxa Orion O6)

On real hardware, measurements will include actual bus latency. Compare
KDAL-mediated I/O against direct register access to quantify abstraction
cost:

```sh
# Direct I/O baseline (platform-specific tool)
./direct_uart_bench 100000 64

# KDAL-mediated
./benchmark uart0 100000 64
```

## Baseline Results (QEMU)

> **Note:** These fields will be populated after QEMU integration testing.

| Device | Block Size | Throughput (KB/s) | Latency (µs/op) |
| ------ | ---------- | ----------------- | --------------- |
| uart0  | 64 B       | TBD               | TBD             |
| i2c0   | 64 B       | TBD               | TBD             |
| spi0   | 64 B       | TBD               | TBD             |
| gpu0   | 64 B       | TBD               | TBD             |

## Optimization Notes

### Current Design Choices

- **Mutex per registry**: Simple and correct. Could be replaced with RCU for
  read-heavy workloads if profiling shows registry lookup as a bottleneck.
- **Ring buffer copy**: The QEMU backend copies data through a ring buffer.
  Real hardware backends bypass this entirely.
- **Single misc device**: All devices share `/dev/kdal` with per-fd selection.
  Per-device character devices could reduce dispatch overhead but increase
  complexity.

### Future Optimizations

- Replace mutex with `rcu_read_lock()` for hot-path device lookup
- Add zero-copy path for DMA-capable backends
- Batch ioctl interface for multi-device operations
- Per-CPU event rings to reduce contention
