cmake_minimum_required(VERSION 3.10)
project(NANOPB_CMAKE_MULTIPLE_OPTIONS_FILES C)

# Both options files are generated, so they have to be tracked as
# dependencies of the nanopb output to exist when protoc runs.
foreach(_name common extra)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${_name}.options"
        COMMAND ${CMAKE_COMMAND} -E copy
                "${CMAKE_CURRENT_SOURCE_DIR}/${_name}.options"
                "${CMAKE_CURRENT_BINARY_DIR}/${_name}.options"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${_name}.options"
    )
endforeach()

set(NANOPB_OPTIONS
    --options-file=${CMAKE_CURRENT_BINARY_DIR}/common.options
    -f ${CMAKE_CURRENT_BINARY_DIR}/extra.options
    -f%s.options)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../extra)
find_package(Nanopb REQUIRED)

nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS simple.proto)
add_executable(simple simple.c ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(simple PRIVATE
    ${NANOPB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)
