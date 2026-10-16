cmake_minimum_required(VERSION 3.10)
project(NANOPB_CMAKE_CUSTOM_OPTIONS_FILE C)

set(CUSTOM_OPTIONS_FILE "${CMAKE_CURRENT_BINARY_DIR}/custom.options")
add_custom_command(
    OUTPUT "${CUSTOM_OPTIONS_FILE}"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/custom.options"
            "${CUSTOM_OPTIONS_FILE}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/custom.options"
)

set(NANOPB_OPTIONS --options-file=${CUSTOM_OPTIONS_FILE})
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../extra)
find_package(Nanopb REQUIRED)

nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS simple.proto)
add_executable(simple simple.c ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(simple PRIVATE
    ${NANOPB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)
