cmake_minimum_required(VERSION 3.20)
project(simple C)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_library(simple-protos STATIC 
	${CMAKE_BINARY_DIR}/src/simple.pb.c
)

add_executable(simple ${CMAKE_BINARY_DIR}/src/simple.c)

target_link_libraries(simple ${CONAN_LIBS} simple-protos)
