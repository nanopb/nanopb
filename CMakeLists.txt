cmake_minimum_required(VERSION 2.8.12)

project(nanopb C)

set(nanopb_VERSION_STRING nanopb-0.4.3)
set(nanopb_SOVERSION 0)

string(REPLACE "nanopb-" "" nanopb_VERSION ${nanopb_VERSION_STRING})

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_STATIC_LIBS "Build static libraries" ON)

option(nanopb_BUILD_RUNTIME "Build the headers and libraries needed at runtime" ON)
option(nanopb_BUILD_GENERATOR "Build the protoc plugin for code generation" ON)
option(nanopb_MSVC_STATIC_RUNTIME "Link static runtime libraries" ON)

if(NOT DEFINED nanopb_PROTOC_PATH)
    set(nanopb_PROTOC_PATH "protoc")
endif()

if(NOT DEFINED CMAKE_DEBUG_POSTFIX)
    set(CMAKE_DEBUG_POSTFIX "d")
endif()

include(GNUInstallDirs)

if(MSVC AND nanopb_MSVC_STATIC_RUNTIME)
    foreach(flag_var
            CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)
endif()

if(NOT DEFINED CMAKE_INSTALL_CMAKEDIR)
    set(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/nanopb")
endif()

if(nanopb_BUILD_GENERATOR)
    set(generator_protos nanopb)

    find_package(Python REQUIRED)
    execute_process(
        COMMAND ${Python_EXECUTABLE} -c
            "from distutils import sysconfig; print(sysconfig.get_python_lib(prefix=''))"
        OUTPUT_VARIABLE PYTHON_INSTDIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    foreach(generator_proto IN LISTS generator_protos)
        string(REGEX REPLACE "([^;]+)" "${PROJECT_SOURCE_DIR}/generator/proto/\\1.proto" generator_proto_file "${generator_proto}")
        string(REGEX REPLACE "([^;]+)" "\\1_pb2.py" generator_proto_py_file "${generator_proto}")
        add_custom_command(
            OUTPUT ${generator_proto_py_file}
            COMMAND ${nanopb_PROTOC_PATH} --python_out=${PROJECT_BINARY_DIR} -I${PROJECT_SOURCE_DIR}/generator/proto ${generator_proto_file}
            DEPENDS ${generator_proto_file}
        )
        add_custom_target("generate_${generator_proto_py_file}" ALL DEPENDS ${generator_proto_py_file})
        install(
            FILES ${PROJECT_BINARY_DIR}/${generator_proto_py_file}
                  ${generator_proto_file}
            DESTINATION ${PYTHON_INSTDIR}/proto/
        )
    endforeach()
endif()

install( FILES generator/proto/_utils.py
         DESTINATION ${PYTHON_INSTDIR}/proto/ )

if( WIN32 )
        install(
            PROGRAMS generator/nanopb_generator.py
                     generator/protoc-gen-nanopb.bat
            DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
else()
        install(
            PROGRAMS generator/nanopb_generator.py
                     generator/protoc-gen-nanopb
            DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
endif()

if(nanopb_BUILD_RUNTIME)
    if(BUILD_SHARED_LIBS)
        add_library(protobuf-nanopb SHARED
            pb.h
            pb_common.h
            pb_common.c
            pb_encode.h
            pb_encode.c
            pb_decode.h
            pb_decode.c)
        set_target_properties(protobuf-nanopb PROPERTIES
            SOVERSION ${nanopb_SOVERSION})
        install(TARGETS protobuf-nanopb EXPORT nanopb-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        target_include_directories(protobuf-nanopb INTERFACE
          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        )
    endif()

    if(BUILD_STATIC_LIBS)
        add_library(protobuf-nanopb-static STATIC
            pb.h
            pb_common.h
            pb_common.c
            pb_encode.h
            pb_encode.c
            pb_decode.h
            pb_decode.c)
        set_target_properties(protobuf-nanopb-static PROPERTIES
            OUTPUT_NAME protobuf-nanopb)
        install(TARGETS protobuf-nanopb-static EXPORT nanopb-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
        target_include_directories(protobuf-nanopb-static INTERFACE
          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        )
    endif()

    configure_file(extra/nanopb-config-version.cmake.in
        nanopb-config-version.cmake @ONLY)

    install(EXPORT nanopb-targets
        DESTINATION ${CMAKE_INSTALL_CMAKEDIR}
        NAMESPACE nanopb::)

    install(FILES extra/nanopb-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/nanopb-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_CMAKEDIR})

    install(FILES pb.h pb_common.h pb_encode.h pb_decode.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
