cmake_minimum_required(VERSION 3.14.0)

project(nanopb C)

set(nanopb_VERSION_STRING nanopb-0.4.8-dev)
set(nanopb_SOVERSION 0)

string(REPLACE "nanopb-" "" nanopb_VERSION ${nanopb_VERSION_STRING})

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_STATIC_LIBS "Build static libraries" ON)

option(nanopb_BUILD_RUNTIME "Build the headers and libraries needed at runtime" ON)
option(nanopb_BUILD_GENERATOR "Build the protoc plugin for code generation" ON)
option(nanopb_MSVC_STATIC_RUNTIME "Link static runtime libraries" ON)

set(nanopb_PYTHON_INSTDIR_OVERRIDE "" CACHE PATH "Override the default python installation directory with the given path")

find_program(nanopb_PROTOC_PATH protoc HINTS generator-bin generator)
if(NOT EXISTS ${nanopb_PROTOC_PATH})
    message(FATAL_ERROR "protoc compiler not found")
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

# Determine Python module installation path
if (NOT nanopb_PYTHON_INSTDIR_OVERRIDE)
    find_package(Python REQUIRED COMPONENTS Interpreter)
    execute_process(
        COMMAND ${Python_EXECUTABLE} -c
            "import os.path, sys, sysconfig; print(sysconfig.get_path('purelib'))"
        OUTPUT_VARIABLE PYTHON_INSTDIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(PYTHON_INSTDIR ${nanopb_PYTHON_INSTDIR_OVERRIDE})
endif()
message(STATUS "Python install dir: ${PYTHON_INSTDIR}")

# Install nanopb generator as Python module 'nanopb'
if(nanopb_BUILD_GENERATOR)
    set(generator_protos nanopb)

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
            DESTINATION ${PYTHON_INSTDIR}/nanopb/generator/proto/
        )
    endforeach()

    install( FILES generator/proto/_utils.py
                   generator/proto/__init__.py
             DESTINATION ${PYTHON_INSTDIR}/nanopb/generator/proto/ )

    install( FILES generator/nanopb_generator.py
                   generator/__init__.py
             DESTINATION ${PYTHON_INSTDIR}/nanopb/generator/ )

    install( FILES generator/__init__.py
             DESTINATION ${PYTHON_INSTDIR}/nanopb/ )

    # Install a script that calls nanopb.generator Python module when invoked
    install(
        PROGRAMS
            extra/script_wrappers/nanopb_generator.py
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    # Install shell/bat script wrappers for invoking nanopb_generator.py.
    # protoc-gen-nanopb is automatically used by protoc when --nanopb_out= option is used.
    if(WIN32)
        # Include the full path to Python executable in Windows .bat scripts, as it is not in PATH on all systems
        file(READ generator/protoc-gen-nanopb.bat FILE_CONTENTS)
        string(REPLACE "python" ${Python_EXECUTABLE} FILE_CONTENTS "${FILE_CONTENTS}")
        file(WRITE ${PROJECT_BINARY_DIR}/protoc-gen-nanopb.bat "${FILE_CONTENTS}")

        file(READ generator/nanopb_generator.bat FILE_CONTENTS)
        string(REPLACE "python" ${Python_EXECUTABLE} FILE_CONTENTS "${FILE_CONTENTS}")
        file(WRITE ${PROJECT_BINARY_DIR}/nanopb_generator.bat "${FILE_CONTENTS}")

        install(
            PROGRAMS
                ${PROJECT_BINARY_DIR}/protoc-gen-nanopb.bat
                ${PROJECT_BINARY_DIR}/nanopb_generator.bat
            DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    else()
        # Linux/Mac scripts currently use python3 from PATH
        install(
            PROGRAMS
                generator/protoc-gen-nanopb
                generator/nanopb_generator
            DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()
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
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
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
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
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
