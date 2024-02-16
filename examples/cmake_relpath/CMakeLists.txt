cmake_minimum_required(VERSION 2.8)
project(NANOPB_CMAKE_SIMPLE C)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../extra)
find_package(Nanopb REQUIRED)

nanopb_generate_cpp(TARGET proto RELPATH proto
	proto/simple.proto proto/sub/unlucky.proto)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -g -O0")

add_executable(simple simple.c)
target_link_libraries(simple proto)
