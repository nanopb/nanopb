# Building

## Baseline prerequisites

Treat `<repo_root>` as the root of this checkout.

For most local work, install:

- `protoc`
- Python 3 packages from `<repo_root>/requirements.txt`: `python3 -m pip install protobuf grpcio-tools`
- `scons` for the main test harness

Extra tools are only needed for the integration you are touching: `cmake`, `meson` + `ninja`, `bazelisk`, `swift`, or PlatformIO.

## Common local commands

### Generate code from a schema

```bash
python3 <repo_root>/generator/nanopb_generator.py path/to/file.proto
```

Enable validation generation when needed with the same flags used by `<repo_root>/examples/validation_simple/Makefile` and `<repo_root>/tests/validation/SConscript`.

### CMake

```bash
cmake -S <repo_root> -B <repo_root>/build
cmake --build <repo_root>/build
```

`<repo_root>/CMakeLists.txt` requires `protoc` and can also install the Python generator package.

### Meson

```bash
meson setup build -Dexamples=enabled
ninja -C build
```

This is the same shape used in `<repo_root>/.github/workflows/meson.yml`.

### Bazel

```bash
bazelisk build //...
bazelisk test --//:nanopb_extension=.pb //...
```

Use the alternate `.nanopb` extension mode only when working on Bazel-specific generation behavior.

### SwiftPM

```bash
swift build
swift test
```

Relevant when changing `<repo_root>/Package.swift`, `<repo_root>/spm_headers/`, or `<repo_root>/spm-test/`.

## Example-oriented quick checks

- `<repo_root>/examples/simple/` is the fastest way to understand the normal generator + runtime loop
- `<repo_root>/examples/validation_simple/` is the fastest way to understand validation-enabled generation
- `<repo_root>/build-tests/` contains packaging/integration checks that mirror CI more than day-to-day development

## Environment notes

- `<repo_root>/tests/SConstruct` prefers clang on macOS when `CC` is not set
- `<repo_root>/CMakeLists.txt` errors out immediately if `protoc` is missing
- The development container under `<repo_root>/.devcontainer/` already includes the common compiler, Python, protobuf, and debugging tools
