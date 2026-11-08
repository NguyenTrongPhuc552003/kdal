## Summary

<!-- What does this PR change, and why? -->

## Component

- [ ] Kernel module (`src/`, `include/`)
- [ ] Tests (`tests/`)
- [ ] CI (`scripts/ci`, `.github/workflows`)
- [ ] Documentation
- [ ] Frozen part (`lang/`, `compiler/`, `editor/`, `tools/`): explain why the change is needed now

## API and ABI impact

- [ ] No API or ABI change
- [ ] Ioctl command added or changed (version bump included)
- [ ] Public header in `include/kdal/` modified
- [ ] debugfs output changed

## Test evidence

<!-- Paste the relevant output: KUnit (KTAP) totals, dmesg, smoke tests, or
a link to the CI run. Name the kernel version and architecture you tested
on. -->

```text

```

## Checklist

- [ ] One logical change per commit; refactors separate from behavior changes
- [ ] Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/) and explain why
- [ ] `./scripts/ci/test.sh` passes locally
- [ ] `./scripts/ci/checkpatch.sh` is clean on changed kernel files
- [ ] New kernel code has KUnit tests in `tests/kunit/`
- [ ] `CHANGELOG.md` (Unreleased) and `docs/` updated if needed
