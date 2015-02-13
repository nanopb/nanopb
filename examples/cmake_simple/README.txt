Nanopb example "simple" using CMake
=======================

This example is the same as the simple nanopb example but built using CMake.

Example usage
-------------

On Linux, create a build directory and then call cmake:

    nanopb/examples/cmake_simple$ mkdir build
    nanopb/examples/cmake_simple$ cd build/
    nanopb/examples/cmake_simple/build$ cmake ..
    nanopb/examples/cmake_simple/build$ make

After that, you can run it with the command: ./simple

#On other platforms, you first have to compile the protocol definition using
#the following command::
#
#  ../../generator-bin/protoc --nanopb_out=. simple.proto
#
#After that, add the following four files to your project and compile:
#
#  simple.c  simple.pb.c  pb_encode.c  pb_decode.c
