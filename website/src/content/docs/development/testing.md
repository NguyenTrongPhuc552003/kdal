---
title: Testing
description: KUnit, integration, userspace, and benchmark testing for KDAL.
---

KDAL has four layers of testing, each targeting a different part of the stack.

## Test Hierarchy

| Layer           | Location                             | Runner                   | Purpose                          |
| --------------- | ------------------------------------ | ------------------------ | -------------------------------- |
| KUnit           | `tests/kunit/`                       | In-kernel (`kunit_tool`) | Internal API correctness         |
| Userspace smoke | `tests/userspace/testapp.c`          | User process             | End-to-end ioctl validation      |
| Integration     | `tests/userspace/integration_test.c` | User process             | Multi-device, power, error paths |
| Benchmark       | `tests/userspace/benchmark.c`        | User process             | Throughput measurement           |

## KUnit Tests

KUnit tests run inside the kernel at boot (or module load) and exercise
internal APIs without userspace involvement.

### Running KUnit

```bash
# Build and run via kunit_tool
./tools/testing/kunit/kunit.py run --kconfig_add CONFIG_KDAL=m

# Or manually:
insmod kdal.ko
dmesg | grep "kunit"
```

### Test Suites

| Suite                | Test Cases                                | Coverage         |
| -------------------- | ----------------------------------------- | ---------------- |
| `kdal_registry_test` | Registration, lookup, duplicate rejection | `registry.c`     |
| `kdal_event_test`    | Emit, poll, overflow, shutdown            | `event.c`        |
| `kdal_power_test`    | State transitions, event emission         | `power.c`        |
| `kdal_chardev_test`  | Open, ioctl dispatch, per-fd state        | `chardev.c`      |
| `kdal_driver_test`   | Probe, remove, read, write ops            | Driver lifecycle |

Total: **38 test cases** across 5 suites.

### Writing KUnit Tests

```c
#include <kunit/test.h>

static void test_register_device(struct kunit *test)
{
    struct kdal_device dev = { .name = "test0" };
    int ret = kdal_register_device(&dev);
    KUNIT_ASSERT_EQ(test, ret, 0);
    kdal_unregister_device(&dev);
}

static struct kunit_case registry_cases[] = {
    KUNIT_CASE(test_register_device),
    {}
};

static struct kunit_suite registry_suite = {
    .name = "kdal_registry",
    .test_cases = registry_cases,
};
kunit_test_suite(registry_suite);
```

## Userspace Smoke Tests

The smoke test exercises the full ioctl path through `/dev/kdal`:

```bash
# Preferred: mirror the CI smoke contract locally
./scripts/ci/test.sh
```

If you want to run the userspace probe manually against a loaded module, build
the helper binary yourself and execute it directly:

```bash
cc -std=c99 -Wall -Wextra -pedantic -D_DEFAULT_SOURCE \
    -o /tmp/testapp tests/userspace/testapp.c
sudo insmod kdal.ko
/tmp/testapp
```

### What it tests

- `KDAL_IOCTL_GET_VERSION` - version query
- `KDAL_IOCTL_LIST_DEVICES` - device enumeration
- `KDAL_IOCTL_GET_INFO` - device info query
- `KDAL_IOCTL_SELECT_DEV` - device selection
- `KDAL_IOCTL_SET_POWER` - power state transitions
- `read()` / `write()` - data path through selected device

## Integration Tests

Integration tests cover multi-device scenarios, error paths, and power
management sequences:

```bash
./tests/userspace/integration_test
```

### Scenarios tested

- Multiple devices selected from different file descriptors
- Power cycling: on → suspend → on → off
- Error injection: invalid device names, out-of-range power states
- Event polling after device add/remove
- Concurrent read/write from multiple processes

## Benchmark

The benchmark tool measures write/read throughput through `/dev/kdal`:

```bash
./tests/userspace/benchmark              # defaults: uart0, 10K iters, 64B
./tests/userspace/benchmark spi0 50000 256
```

| Parameter  | Default  | Description                 |
| ---------- | -------- | --------------------------- |
| Device     | `uart0`  | Target device name          |
| Iterations | 10000    | Number of write/read cycles |
| Block size | 64 bytes | Payload per operation       |

Uses `clock_gettime(CLOCK_MONOTONIC)` for nanosecond resolution.

## Compiler Tests

The compiler can be tested by compiling sample `.kdc` files and verifying
the generated output:

```bash
# Compile an example
./build/release/compiler/kdalc examples/kdc_hello/uart_hello.kdc -o /tmp/out

# Verify output exists
ls /tmp/out/UartHello.c /tmp/out/Makefile.kbuild
```

## Auto-Generated Tests

`kdality test-gen` creates KUnit test stubs from any `.kdc` driver:

```bash
kdality test-gen examples/led_driver.kdc -o tests/generated/
```

This generates one `KUNIT_CASE` per event handler found in the driver,
with setup/teardown fixtures and suite registration.

## Development Test Script

The `test.sh` script mirrors CI stages locally with numbered PASS/FAIL output
and saves reports under the selected variant tree, for example
`build/release/test/<target>_<date>.log`.

```bash
# Run all tests
./scripts/dev/test.sh --variant release all

# Test individual targets
./scripts/dev/test.sh --variant release kdalc       # Compiler binary + sample compilation
./scripts/dev/test.sh --variant release kdality     # CLI subcommands
./scripts/dev/test.sh --variant release cppcheck    # C static analysis (mirrors CI)
./scripts/dev/test.sh --variant release shellcheck  # Shell script linting (mirrors CI)
./scripts/dev/test.sh --variant release structure   # Project structure validation
./scripts/dev/test.sh --variant release e2e         # End-to-end test suites
./scripts/dev/test.sh help        # List all targets
```

## CI Pipeline

GitHub Actions workflows (`.github/workflows/`) run on every push:

| Workflow | Jobs                                                   |
| -------- | ------------------------------------------------------ |
| `build`  | CMake compile matrix (kdalc, kdality, website, vscode) |
| `ci`     | cppcheck, shellcheck, project structure, doc links     |
| `test`   | KUnit + userspace tests                                |
| `e2e`    | User journey, developer journey, compiler, packaging   |
| `codeql` | Security analysis (CodeQL)                             |
| `docs`   | Documentation site deployment (Vercel)                 |
