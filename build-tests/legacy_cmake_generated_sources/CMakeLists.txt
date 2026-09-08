cmake_minimum_required(VERSION 3.5)
project(NANOPB_CMAKE_GENERATED_SOURCES C)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../extra)
find_package(Nanopb REQUIRED)
include_directories(${NANOPB_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

# Regression for #1104: reusing the output variables must not mark runtime
# sources as generated, because the clean target would then delete them.
set(NANOPB_RUNTIME_FILES ${NANOPB_SRCS} ${NANOPB_HDRS})
nanopb_generate_cpp(NANOPB_SRCS NANOPB_HDRS ../legacy_cmake_simple/simple.proto)
foreach(source_file ${NANOPB_RUNTIME_FILES})
  get_source_file_property(is_generated ${source_file} GENERATED)
  if(is_generated)
    message(FATAL_ERROR "Runtime source marked as generated: ${source_file}")
  endif()
endforeach()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -g -O0")
add_executable(simple ../legacy_cmake_simple/simple.c ${NANOPB_SRCS} ${NANOPB_HDRS})
