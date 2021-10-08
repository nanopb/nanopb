cmake_minimum_required(VERSION 3.11)
project(NANOPB_CMAKE_FETCHCONTENT C)

include(FetchContent)

FetchContent_Declare(
	nanopb
	GIT_REPOSITORY https://github.com/xor-gate/nanopb
	GIT_TAG cmake_fetchcontent_support
	#URL https://github.com/nanopb/nanopb/archive/refs/tags/0.4.5.zip
	#URL_HASH SHA256=5fbfc69bddf135d6894ee0b35eaabf2fafa3f7f44fa4fe8f7c99222288c37189
)

set(NANOPB_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(NANOPB_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

set(Nanopb_LIBRARIES protobuf-nanopb-static)

FetchContent_MakeAvailable(nanopb)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -g -O0")

set(CMAKE_MODULE_PATH ${nanopb_SOURCE_DIR}/extra)
find_package(Nanopb REQUIRED)

nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS simple.proto)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS}
    PROPERTIES GENERATED TRUE)

add_executable(simple simple.c ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(simple ${Nanopb_LIBRARIES})
