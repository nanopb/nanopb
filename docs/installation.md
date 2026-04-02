# Installation

The nanopb project consists of two parts:

1. Generator, which runs on developer's computer.
2. C library, which runs on the embedded system.

The generator is Python based and is run during project compilation to convert `.proto` files into C source code and headers. It can be used on macOS, Windows, Linux, BSD, and other platforms where Python 3 is available.

The C library is compiled for the embedded target and handles the runtime processing of the messages.
It is compatible with practically any platform that has a C99-conformant C compiler available.

## Installing the generator

There are three ways to install the nanopb generator:

1. Install depedencies and generator from `pip`.
2. Install dependencies, then run generator from nanopb source folder.
3. Run generator from nanopb binary package without installation.

### Installing from pip

This is the easiest way to get latest version of nanopb generator.
It is recommended to use a [Python virtual environment (venv)](https://packaging.python.org/en/latest/guides/installing-using-pip-and-virtual-environments/) for the installation, as that separates the packages installed from any other projects on the same computer.

To create a virtual environment and install the generator, use:

    python3 -m venv venv
    venv/bin/pip install nanopb grpcio_tools

By default the latest version is installed.
You can optionally specify the version you require by using e.g. `nanopb==1.0.0`.

The generator is then available at `venv/bin/nanopb_generator` and can be used to convert `.proto` files. It is not necessary to manually activate the virtual environment, as the installed script automatically uses it.

    $ venv/bin/nanopb_generator test.proto
    Writing to test.pb.h and test.pb.c

### Running from nanopb source folder

The generator script is in [generator](../generator/) folder in nanopb source distribution.
To run it, you need to have [Python 3](https://www.python.org/downloads/), [python-protobuf](https://pypi.org/project/protobuf/) and the protobuf compiler protoc available. Protoc can be installed either [separately](https://protobuf.dev/installation/) or as part of the `grpcio_tools` package.

It doesn't matter much which method you use to install the dependencies, but using `pip` generally gives the most up to date versions if you need the latest protobuf features:

* Preferred for **any system**: Install Python 3, then use `venv/bin/pip install protobuf grpcio_tools`
* Alternative for **Ubuntu / Debian**: Use `apt install python3-protobuf protobuf-compiler`
* Alternative for **macOS**: Install [homebrew](https://brew.sh/), then use `brew install protobuf`

The generator can then be run through python:

    $ python3 nanopb/generator/nanopb_generator.py test.proto
    Writing to test.pb.h and test.pb.c

By default some internal files are generated and cached under the `generator/proto` folder.
If you require that no extra files are created into the source folder, you can set environment variable `NANOPB_PB2_TEMP_DIR` to configure a custom build folder.

### Running from binary package

Nanopb binary packages are available for Linux, Windows and macOS.
They include the Python interpreter and the protobuf libraries.
The binary package includes generator source code in `generator` folder, and the binary version under `generator-bin`:

    $ nanopb/generator-bin/nanopb_generator test.proto
    Writing to test.pb.h and test.pb.c

### Troubleshooting generator installation

The most common problem with nanopb generator installation is that some `protoc` versions are incompatible with some `python-protobuf` versions. Python-protobuf 3.20.0 and later [have improved compatibility](https://protobuf.dev/support/cross-version-runtime-guarantee/#python).

There may be multiple copies of `python-protobuf` and `protoc` installed on your system.
To check which paths nanopb is using, give `-vV` (or `--verbose --version`) argument:

    $ nanopb_generator -Vv
    nanopb-1.0.0
    Using grpcio-tools protoc from /.../venv/lib/python3.13/site-packages/grpc_tools/protoc.py
    libprotoc 31.1
    protoc builtin include path: ['/.../venv/lib/python3.13/site-packages/nanopb/generator/proto', '/.../venv/lib/python3.13/site-packages/grpc_tools/_proto']
    Python version 3.13.5 (main, Jun 25 2025, 18:55:22) [GCC 14.2.0]
    Using python-protobuf from /.../venv/lib/python3.13/site-packages/google/protobuf/__init__.py
    Python-protobuf version: 6.33.6

The compatibility matrix of `protoc` vs. `python-protobuf` versions is available in Google's [Protobuf Version Support document](https://protobuf.dev/support/version-support/#python).

## Building the C library

The nanopb C library is built and linked with your own C source code.
There are three ways to include it in the build:

1. Compile nanopb as part of your project build.
2. Compile a nanopb static library and link it into your project.
3. Compile a nanopb shared library and use runtime dynamic linking.

For embedded targets, either 1. or 2. is usually preferred.

For any of these you need the nanopb source code, which you can either download separately, or include as a [git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules).

### Compile as part of your project build

To build nanopb as part of your project, add the source code files and include directory to project configuration:

* Add source files from [nanopb/src/](../src) into the project
* Add [nanopb/include](../include) to C compiler include path

### Compile as a static library

A static library can be compiled using the CMake build system:

    $ cmake -B build
    $ cmake --build build
    ...
    [100%] Built target protobuf-nanopb-static

For embedded targets you need to specify cross compilation options:

    $ cmake -B build -DCMAKE_C_FLAGS="-Os -Wall" -DCMAKE_C_COMPILER="arm-none-eabi-gcc" -DCMAKE_TRY_COMPILE_TARGET_TYPE="STATIC_LIBRARY"
    $ cmake --build build    # Add -v to see compilation commands for troubleshooting
    ...
    [100%] Built target protobuf-nanopb-static

This produces `build/libprotobuf-nanopb.a`, which you can then link into your own project.

### Compile as a shared library

Shared libraries are a reasonable option for platforms with a higher level operating system where dynamic linking is supported. CMake can be configured to build a dynamic library by setting `BUILD_SHARED_LIBS`:

    $ cmake -B build -DBUILD_SHARED_LIBS=1
    $ cmake --build build
    ...
    [ 50%] Linking C shared library libprotobuf-nanopb.so
    ...

After this you can link against the `build/libprotobuf-nanopb.so` and copy it into the shared library folder on the target device.

## Build system integration

There exist community-maintained support to help integrating nanopb with various build systems.

For example [PlatformIO build rules](build_platformio.md) support automatically installing the required Python dependencies and running the generator.
