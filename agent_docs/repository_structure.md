# Repository structure

Treat `<repo_root>` as the root of this checkout.

## Top-level map

- `<repo_root>/pb*.c` and `<repo_root>/pb*.h` — handwritten runtime sources and public headers
- `<repo_root>/generator/` — Python generator, validator generator, plugin wrappers, and generator-owned proto files
- `<repo_root>/tests/` — the main executable spec; every subdirectory is usually one focused scenario or regression
- `<repo_root>/examples/` — consumer-facing usage examples and integration samples
- `<repo_root>/extra/` — reusable integration assets for downstream build systems
- `<repo_root>/build-tests/` — CI-oriented packaging and integration test fixtures
- `<repo_root>/docs/` — user docs and reference material
- `<repo_root>/.github/workflows/` — the authoritative CI matrix for supported environments

## Examples worth knowing

- `<repo_root>/examples/simple/` — smallest end-to-end encode/decode example
- `<repo_root>/examples/validation_simple/` — validation-enabled example
- `<repo_root>/examples/cmake_simple/`, `<repo_root>/examples/meson_simple/`, and `<repo_root>/examples/conan_dependency/` — downstream integration references
- `<repo_root>/examples/platformio/` — PlatformIO packaging and generator integration

## Tests worth knowing

- `<repo_root>/tests/common/` — shared harness pieces
- `<repo_root>/tests/regression/` — issue-driven regressions; check here before changing behavior
- `<repo_root>/tests/validation/` — validation feature coverage
- `<repo_root>/tests/site_scons/` — SCons platform adapters and generator integration for the test harness

## Build/integration files worth knowing

- `<repo_root>/CMakeLists.txt`
- `<repo_root>/meson.build`
- `<repo_root>/BUILD.bazel`
- `<repo_root>/Package.swift`
- `<repo_root>/conanfile.py`
- `<repo_root>/extra/nanopb.mk`

## Docs worth knowing

- `<repo_root>/docs/index.md` — overview
- `<repo_root>/docs/concepts.md` — type mapping and stream model
- `<repo_root>/docs/reference.md` — API and generator options
- `<repo_root>/docs/validation.md` — validation feature behavior
