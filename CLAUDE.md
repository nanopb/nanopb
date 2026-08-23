# CLAUDE.md

This file orients AI coding agents to the nanopb codebase. Treat `<repo_root>` as the root of this checkout. For API details and end-user usage, prefer the authoritative docs under `<repo_root>/docs/`.

## Why this project exists

Nanopb is a small Protocol Buffers implementation for memory-constrained systems, especially microcontrollers. The project keeps the runtime in portable C and moves schema interpretation into a generator, so most protobuf complexity is handled ahead of time instead of at runtime.

Two architectural choices matter for most tasks:

- The runtime is intentionally small and split by responsibility: common support in `<repo_root>/pb_common.c`, encoding in `<repo_root>/pb_encode.c`, decoding in `<repo_root>/pb_decode.c`, and optional validation in `<repo_root>/pb_validate.c`.
- `.proto` files are turned into C structs and descriptor tables by the Python generator in `<repo_root>/generator/nanopb_generator.py`; the generated code is the bridge between protobuf schemas and the C runtime.

## What is here

### Languages and tooling

- Runtime: ANSI C in `<repo_root>/pb*.c` and `<repo_root>/pb*.h`
- Generator: Python 3 in `<repo_root>/generator/`
- Main test runner: SCons from `<repo_root>/tests/SConstruct`
- Supported integration/build systems: CMake, Meson, Bazel, Conan, Make, SwiftPM, PlatformIO
- Core Python deps: `protobuf`, `grpcio-tools` (`<repo_root>/requirements.txt`)

### High-level layout

- `<repo_root>/pb.h` — compile-time feature flags and common types
- `<repo_root>/pb_encode.*` / `<repo_root>/pb_decode.*` — public runtime APIs and implementations
- `<repo_root>/pb_validate.*` — optional declarative validation runtime
- `<repo_root>/generator/` — schema-to-C generator, validator generator, packaged entrypoints
- `<repo_root>/generator/proto/` — generator-owned protobuf definitions such as `nanopb.proto` and `validate.proto`
- `<repo_root>/tests/` — canonical behavior and regression coverage
- `<repo_root>/examples/` — minimal consumer-facing examples for major integration styles
- `<repo_root>/extra/` — reusable integration files (`nanopb.mk`, CMake helpers, Bazel support)
- `<repo_root>/build-tests/` — CI-only integration checks for packaging/build systems
- `<repo_root>/docs/` — authoritative user and API documentation

### Where to look first

- Runtime bug or feature: start with `<repo_root>/pb.h` plus the relevant `<repo_root>/pb_*.c`
- Generator bug or generated-code shape issue: `<repo_root>/generator/nanopb_generator.py` and the nearest case in `<repo_root>/tests/`
- Validation work: `<repo_root>/pb_validate.*`, `<repo_root>/generator/nanopb_validator.py`, `<repo_root>/tests/validation/`
- Build-system integration: matching files in `<repo_root>/CMakeLists.txt`, `<repo_root>/meson.build`, `<repo_root>/BUILD.bazel`, `<repo_root>/Package.swift`, or `<repo_root>/extra/`

## How to work in this repo

### Setup and code generation

- Install the baseline generator deps: `python3 -m pip install protobuf grpcio-tools`
- Install `protoc`; both the generator and several build systems expect it to be available
- Generate C from schemas with `python3 <repo_root>/generator/nanopb_generator.py your_file.proto`

### Build and run

- Canonical library/example build for local development: `cmake -S <repo_root> -B <repo_root>/build && cmake --build <repo_root>/build`
- Canonical example-first sanity check: `cd <repo_root>/examples/simple && make && ./simple`
- Meson, Bazel, PlatformIO, SwiftPM, and Conan support are real and CI-covered, but only exercise the one you changed

### Tests and verification

- The default verification path is `cd <repo_root>/tests && scons`
- Use that test suite for almost any runtime or generator change; it is the main CI smoke test and the densest source of expected behavior
- There is no single repo-wide lint or typecheck command; local quality checks come mainly from warning-as-error compiler builds in SCons and optional valgrind coverage in the same harness
- If you touch integration-specific files, also run the matching workflow command:
  - CMake: `cmake -S ... -B ... && cmake --build ...`
  - Meson: `meson setup build -Dexamples=enabled && ninja -C build`
  - Bazel: `bazelisk test --//:nanopb_extension=.pb //...`
  - SwiftPM: `swift build && swift test`
  - PlatformIO: use `<repo_root>/.github/workflows/platformio_tests.yml` as the source of truth
- On macOS, prefer `scons CC=clang CXX=clang++`; `<repo_root>/tests/SConstruct` does this automatically when `CC` is unset

### Non-obvious workflow details

- The runtime is heavily configuration-driven via macros in `<repo_root>/pb.h`; changes there can affect many tests and integrations
- The test harness adds strict compiler flags and optional valgrind checks in `<repo_root>/tests/SConstruct`, so failures may be toolchain-specific rather than functional
- Validation code generation requires both generator and runtime changes to stay in sync
- Prefer existing examples and regression tests over inventing new ad hoc verification flows

## Read more only when needed

- `<repo_root>/agent_docs/architecture.md` — runtime/generator split, validation path, and key design tradeoffs
- `<repo_root>/agent_docs/building.md` — concise build/setup commands across supported build systems
- `<repo_root>/agent_docs/testing.md` — which tests to run for which kinds of changes
- `<repo_root>/agent_docs/repository_structure.md` — where major directories and examples fit
- `<repo_root>/docs/reference.md` — public API details
- `<repo_root>/docs/concepts.md` and `<repo_root>/docs/validation.md` — protocol mapping and validation behavior
