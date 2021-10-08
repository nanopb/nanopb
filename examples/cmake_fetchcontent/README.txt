Nanopb example "simple" using CMake FetchContent module
=======================

This example is the same as the simple nanopb example but built using CMake FetchContent module
introduced in 3.11. Then you can make nanopb available into your existing CMake project.

Example usage
-------------

On Linux, create a build directory and then call cmake:

    nanopb/examples/cmake_fetchcontent$ mkdir build
    nanopb/examples/cmake_fetchcontent$ cd build/
    nanopb/examples/cmake_fetchcontent/build$ cmake ..
    nanopb/examples/cmake_fetchcontent/build$ make

After that, you can run it with the command: ./simple

On other platforms supported by CMake, refer to CMake instructions.
