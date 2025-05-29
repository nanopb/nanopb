cmake_minimum_required(VERSION 3.14.0)

project(nanopb VERSION 1.0.0 LANGUAGES C)

set(nanopb_VERSION_STRING ${PROJECT_NAME}-${${PROJECT_NAME}_VERSION}-dev)
set(nanopb_SOVERSION 0)

string(REPLACE "nanopb-" "" nanopb_VERSION ${nanopb_VERSION_STRING})

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_STATIC_LIBS "Build static libraries" ON)

option(nanopb_BUILD_RUNTIME "Build the headers and libraries needed at runtime" ON)
option(nanopb_BUILD_GENERATOR "Build the protoc plugin for code generation" ON)
option(nanopb_MSVC_STATIC_RUNTIME "Link static runtime libraries" ON)

set(nanopb_PYTHON_INSTDIR_OVERRIDE "" CACHE PATH "Override the default python installation directory with the given path")

find_program(nanopb_PROTOC_PATH protoc PATHS generator-bin generator NO_DEFAULT_PATH)
find_program(nanopb_PROTOC_PATH protoc)
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
    file(TO_CMAKE_PATH "${Python_SITELIB}" PYTHON_INSTDIR)
else()
    set(PYTHON_INSTDIR ${nanopb_PYTHON_INSTDIR_OVERRIDE})
endif()
message(STATUS "Python install dir: ${PYTHON_INSTDIR}")

# Package nanopb generator as Python module 'nanopb'
if(nanopb_BUILD_GENERATOR)
    # Copy Python code files related to the generator
    add_custom_target(nanopb_generator ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${PROJECT_BINARY_DIR}/nanopb/generator/proto

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/generator/proto/_utils.py
            ${PROJECT_SOURCE_DIR}/generator/proto/__init__.py
            ${PROJECT_SOURCE_DIR}/generator/proto/nanopb.proto
            ${PROJECT_BINARY_DIR}/nanopb/generator/proto

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/generator/nanopb_generator.py
            ${PROJECT_SOURCE_DIR}/generator/__init__.py
            ${PROJECT_BINARY_DIR}/nanopb/generator

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/generator/__init__.py
            ${PROJECT_BINARY_DIR}/nanopb

        COMMAND ${nanopb_PROTOC_PATH}
            --python_out=${PROJECT_BINARY_DIR}/nanopb/generator/proto
            -I${PROJECT_SOURCE_DIR}/generator/proto
            ${PROJECT_SOURCE_DIR}/generator/proto/nanopb.proto
    )



    # Install Python module files
    install(
        DIRECTORY ${PROJECT_BINARY_DIR}/nanopb
        DESTINATION ${PYTHON_INSTDIR}
        FILES_MATCHING
        PATTERN *.py
        PATTERN *.proto
        PATTERN __pycache__ EXCLUDE
    )

    # Generate a wrapper script that calls nanopb.generator Python module when invoked
    configure_file(
        extra/script_wrappers/nanopb_generator.py.in
        ${PROJECT_BINARY_DIR}/nanopb_generator.py
    )
    install(
        PROGRAMS ${PROJECT_BINARY_DIR}/nanopb_generator.py
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
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nanopb>
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
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nanopb>
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
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nanopb)
endif()
