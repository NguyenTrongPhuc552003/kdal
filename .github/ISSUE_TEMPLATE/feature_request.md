---
name: Feature request
about: Propose a new capability or a change to KDAL
title: "feat: "
labels: enhancement
---

## Problem statement

<!-- What problem does this solve, and for whom? Why is the current
behavior not enough? -->

## Proposed capability

<!-- Describe the change. For the kernel module, say which layer it touches:
control plane, core runtime, drivers or backends. -->

## Affected component

<!-- New work goes into the kernel module. The language, compiler, editor
plugins and tools are frozen since 2026-11-01 and take no new features
unless the thesis needs them. -->

- [ ] Kernel module (`src/`, `include/`)
- [ ] Tests or CI (`tests/`, `scripts/ci`)
- [ ] Frozen: language, compiler, editor plugins or tools (`lang/`, `compiler/`, `editor/`, `tools/`)
- [ ] Documentation or other

## Public API and ABI impact

<!-- Does this add or change an ioctl, a public header in `include/kdal/`,
or the debugfs interface? New ioctls need a version bump (see
CONTRIBUTING.md, Architecture Rules). -->

## Test strategy

<!-- How will this be verified? New kernel code needs KUnit tests; new
ioctl paths need userspace coverage (see CONTRIBUTING.md, Testing
Requirements). -->

## Alternatives considered
