cmake_minimum_required(VERSION 3.14)
project(validation_simple C)

# Find nanopb
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_MODULE_PATH})
find_package(Nanopb REQUIRED)

# Generate protobuf files with validation.
# Defining NANOPB_VALIDATE_OPTIONS switches on the nanopb-validate plugin;
# an empty value means "validate, with no special envelope handling".
set(NANOPB_VALIDATE_OPTIONS "")
nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS
    validation_simple.proto)

# Include directories
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${NANOPB_INCLUDE_DIRS})

# Build the example
add_executable(validation_simple
    validation_simple.c
    ${PROTO_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pb_validate.c
)

# Link with nanopb
target_link_libraries(validation_simple ${NANOPB_LIBRARIES})
