cmake_minimum_required(VERSION 2.8)

project(nanopb C)

set(nanopb_VERSION_STRING nanopb-0.3.7-dev)

string(REPLACE "nanopb-" "" nanopb_VERSION ${nanopb_VERSION_STRING})

option(nanopb_MSVC_STATIC_RUNTIME "Link static runtime libraries" ON)

if(NOT DEFINED CMAKE_DEBUG_POSTFIX)
    set(CMAKE_DEBUG_POSTFIX "d")
endif()

if(MSVC AND nanopb_MSVC_STATIC_RUNTIME)
    foreach(flag_var
            CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)
endif()

add_library(libprotobuf-nanopb STATIC
    pb.h
    pb_common.h
    pb_common.c
    pb_encode.h
    pb_encode.c
    pb_decode.h
    pb_decode.c)

target_include_directories(libprotobuf-nanopb INTERFACE
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

include(GNUInstallDirs)

if(NOT DEFINED CMAKE_INSTALL_CMAKEDIR)
    set(CMAKE_INSTALL_CMAKEDIR "lib/cmake/nanopb")
endif()

install(TARGETS libprotobuf-nanopb EXPORT nanopb-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(EXPORT nanopb-targets
    DESTINATION ${CMAKE_INSTALL_CMAKEDIR}
    NAMESPACE nanopb::)

configure_file(extra/nanopb-config-version.cmake.in
    nanopb-config-version.cmake @ONLY)

install(FILES extra/nanopb-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/nanopb-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_CMAKEDIR})

install(FILES pb.h pb_common.h pb_encode.h pb_decode.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
