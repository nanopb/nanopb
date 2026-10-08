cmake_minimum_required(VERSION 3.10)
project(NANOPB_CMAKE_OPTIONS_LIST C CXX)

# NANOPB_OPTIONS given as a CMake list, with more options appended
# through find_package() COMPONENTS.
set(NANOPB_OPTIONS --source-extension=.cpp --header-extension=.hpp)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../extra)
find_package(Nanopb REQUIRED COMPONENTS cpp-descriptors)
include_directories(${NANOPB_INCLUDE_DIRS})

nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS simple.proto)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -g -O0")

add_executable(simple simple.cpp ${PROTO_SRCS} ${PROTO_HDRS})
