Nanopb - Protocol Buffers for Embedded Systems
==============================================

[![Build Status](https://travis-ci.org/nanopb/nanopb.svg?branch=master)](https://travis-ci.org/nanopb/nanopb)

Nanopb is a small code-size Protocol Buffers implementation in ansi C. It is
especially suitable for use in microcontrollers, but fits any memory
restricted system.

* **Homepage:** https://jpa.kapsi.fi/nanopb/
* **Documentation:** https://jpa.kapsi.fi/nanopb/docs/
* **Downloads:** https://jpa.kapsi.fi/nanopb/download/
* **Forum:** https://groups.google.com/forum/#!forum/nanopb



Using the nanopb library
------------------------
To use the nanopb library, you need to do two things:

1. Compile your .proto files for nanopb, using `protoc`.
2. Include *pb_encode.c*, *pb_decode.c* and *pb_common.c* in your project.

The easiest way to get started is to study the project in "examples/simple".
It contains a Makefile, which should work directly under most Linux systems.
However, for any other kind of build system, see the manual steps in
README.txt in that folder.



Using the Protocol Buffers compiler (protoc)
--------------------------------------------
The nanopb generator is implemented as a plugin for the Google's own `protoc`
compiler. This has the advantage that there is no need to reimplement the
basic parsing of .proto files. However, it does mean that you need the
Google's protobuf library in order to run the generator.

If you have downloaded a binary package for nanopb (either Windows, Linux or
Mac OS X version), the `protoc` binary is included in the 'generator-bin'
folder. In this case, you are ready to go. Simply run this command:

    generator-bin/protoc --nanopb_out=. myprotocol.proto

However, if you are using a git checkout or a plain source distribution, you
need to provide your own version of `protoc` and the Google's protobuf library.
On Linux, the necessary packages are `protobuf-compiler` and `python-protobuf`.
On Windows, you can either build Google's protobuf library from source (see section below) or use
one of the binary distributions of it. In either case, if you use a separate
`protoc`, you need to manually give the path to the nanopb generator to the `protoc-gen-nanopb` 
plugin, as follows:

    protoc --plugin=protoc-gen-nanopb=nanopb/generator/protoc-gen-nanopb --nanopb_out=. myprotocol.proto

Note that the above `protoc`-based commands are the 1-command versions of a 2-command process, as described in the ["Nanopb: Basic concepts" document under the section "Compiling .proto files for nanopb"](https://jpa.kapsi.fi/nanopb/docs/concepts.html#compiling-proto-files-for-nanopb). Here is the 2-command process:

    protoc -omyprotocol.pb myprotocol.proto
    python nanopb/generator/nanopb_generator.py myprotocol.pb



Building [Google's protobuf library](https://github.com/protocolbuffers/protobuf) from source
---------------------------------------------------------------------------------------------
When building Google's protobuf library from source, be sure to follow both the C++ installation instructions *and* the Python installation instructions, as *both* are required:
1. [Protobuf's C++ build & installation instructions](https://github.com/protocolbuffers/protobuf/tree/master/src)
2. [Protobuf's Python build & installation instructions](https://github.com/protocolbuffers/protobuf/tree/master/python)  
- See additional reading [here](https://github.com/nanopb/nanopb/issues/417#issuecomment-517619517) and [here](https://stackoverflow.com/questions/57367265/how-to-compile-nanopb-proto-file-into-h-and-c-files-using-nanopb-and-protobuf/57367543#57367543).



Running the tests
-----------------
If you want to perform further development of the nanopb core, or to verify
its functionality using your compiler and platform, you'll want to run the
test suite. The build rules for the test suite are implemented using Scons,
so you need to have that installed (ex: `sudo apt install scons` on Ubuntu). To run the tests:

    cd tests
    scons

This will show the progress of various test cases. If the output does not
end in an error, the test cases were successful.

Note: Mac OS X by default aliases 'clang' as 'gcc', while not actually
supporting the same command line options as gcc does. To run tests on
Mac OS X, use: `scons CC=clang CXX=clang`. Same way can be used to run
tests with different compilers on any platform.

For embedded platforms, there is currently support for running the tests
on STM32 discovery board and [simavr](https://github.com/buserror/simavr)
AVR simulator. Use `scons PLATFORM=STM32` and `scons PLATFORM=AVR` to run
these tests.


Build systems and integration
-----------------------------
Nanopb C code itself is designed to be portable and easy to build
on any platform. Often the bigger hurdle is running the generator which
takes in the `.proto` files and outputs `.pb.c` definitions.

There exist build rules for several systems:

* **Makefiles**: `extra/nanopb.mk`, see `examples/simple`
* **CMake**: `extra/FindNanopb.cmake`, see `examples/cmake`
* **SCons**: `tests/site_scons` (generator only)
* **Bazel**: `BUILD` in source root
* **Conan**: `conanfile.py` in source root
* **PlatformIO**: https://platformio.org/lib/show/431/Nanopb
* **PyPI/pip**: https://pypi.org/project/nanopb/

And also integration to platform interfaces:

* **Arduino**: http://platformio.org/lib/show/1385/nanopb-arduino
