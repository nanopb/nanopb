# Testing

## Default test command

Treat `<repo_root>` as the root of this checkout.

For most runtime and generator changes, use:

```bash
cd <repo_root>/tests
scons
```

`<repo_root>/.github/workflows/trigger_on_code_change.yml` uses this as the primary smoke test, and `<repo_root>/tests/SConstruct` is the canonical local harness.

## Which tests to run

### Runtime or generator changes

Run the default `scons` suite first. The test tree covers encoding, decoding, callbacks, oneofs, proto3, regressions, validation, naming, and many generator edge cases.

### Validation changes

Make sure `<repo_root>/tests/validation/` still passes, because validation spans schema options, generator output, and runtime support.

### Build-system or packaging changes

Run the command that matches the integration you edited:

- CMake: the commands from `<repo_root>/.github/workflows/cmake.yml`
- Meson: the commands from `<repo_root>/.github/workflows/meson.yml`
- Bazel: the commands from `<repo_root>/.github/workflows/bazel.yml`
- PlatformIO: the flow in `<repo_root>/.github/workflows/platformio_tests.yml`
- SwiftPM: `swift build && swift test` from `<repo_root>/.github/workflows/ios_swift_tests.yml`

### Toolchain-sensitive changes

If a change affects portability, integer widths, warnings, or compile-time macros in `<repo_root>/pb.h`, copy the relevant matrix from `<repo_root>/.github/workflows/compiler_tests.yml` instead of relying on one compiler.

## Non-obvious harness behavior

- `<repo_root>/tests/SConstruct` adds strict warning flags and often treats warnings as errors
- If valgrind is available, the harness may use it unless `NOVALGRIND=1` is set
- Some embedded/simulator coverage is available through `PLATFORM=AVR`, `PLATFORM=MIPS`, `PLATFORM=MIPSEL`, `PLATFORM=RISCV64`, and `PLATFORM=STM32`
- The harness may switch to a compatibility system header path when standard C headers are unavailable

There is no separate repo-wide lint or type-check target to run first; the practical local checks are compiler warnings-as-errors plus whatever valgrind-backed coverage the SCons harness enables.

## Coverage and focused runs

- `<repo_root>/tests/Makefile` provides `make coverage`
- For narrow investigation, start from the closest test directory under `<repo_root>/tests/` and its `SConscript`, but prefer the full suite before finalizing a behavior change
