cmake_minimum_required(VERSION 2.8.9 FATAL_ERROR)

project(nanopb C)

set(nanopb_VERSION_STRING nanopb-0.3.9-dev)

string(REPLACE "nanopb-" "" nanopb_VERSION ${nanopb_VERSION_STRING})

option(nanopb_BUILD_RUNTIME "Build the headers and libraries needed at runtime" ON)
option(nanopb_BUILD_GENERATOR "Build the protoc plugin for code generation" ON)
option(nanopb_MSVC_STATIC_RUNTIME "Link static runtime libraries" ON)

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
    set(CMAKE_INSTALL_CMAKEDIR "lib/cmake/nanopb")
endif()

set(generator_protos options)

if(nanopb_BUILD_GENERATOR)
    set(python_versions 2.7 3.4 3.5 3.6 3.7)
    set(python_instdirs "")
    foreach(python_version IN LISTS python_versions)
        set(python_exe "")
        string(REPLACE "." ";" python_version_list ${python_version})
        list(GET python_version_list 0 python_version_major)
        set(python_possibles python${python_version} python${python_version_major} python)
        foreach(python_possible IN LISTS python_possibles)
            execute_process(
                COMMAND ${python_possible} -c
                    "import sys; print('.'.join([str(x) for x in sys.version_info[:2]]))"
                OUTPUT_VARIABLE python_version_actual
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(python_version EQUAL python_version_actual)
                set(python_exe ${python_possible})
                break()
            endif()
        endforeach()
        if(python_exe)
            message(STATUS "Found python: " ${python_exe})
            execute_process(
                COMMAND ${python_exe} -c
                    "from distutils import sysconfig; print(sysconfig.get_python_lib(prefix='${CMAKE_INSTALL_PREFIX}'))"
                OUTPUT_VARIABLE python_instdir
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            list(APPEND python_instdirs ${python_instdir})
            message(STATUS "Installing generator libraries to: " ${python_instdir})
        endif()
    endforeach()

    set(PROTOBUF_DIR "" CACHE STRING "Location of the protobuf directory")
    set(PROTOBUF_INC_DIR "" CACHE STRING "Location of the protobuf include directory")
    if (NOT "${PROTOBUF_INC_DIR}" STREQUAL "")
        set(protoc_FLAGS "-I${PROTOBUF_INC_DIR}")
    elseif(NOT "${PROTOBUF_DIR}" STREQUAL "")
        set(protoc_FLAGS "-I${PROTOBUF_DIR}/include")
    else()
        include(FindPkgConfig)
        pkg_search_module(protobuf protobuf)
        if(protobuf_FOUND)
            string(REGEX REPLACE "([^;]+)" "-I\\1" protoc_FLAGS "${protobuf_INCLUDE_DIRS}")
        else()
            set(protoc_FLAGS "-I/usr/include")
        endif()
    endif()
    message(STATUS "Protoc Flags: " ${protoc_FLAGS})

    foreach(generator_proto IN LISTS generator_protos)
        string(REGEX REPLACE "([^;]+)" "${PROJECT_SOURCE_DIR}/generator/nanopb/\\1.proto" generator_proto_file "${generator_proto}")
        string(REGEX REPLACE "([^;]+)" "${PROJECT_BINARY_DIR}/\\1_pb2.py" generator_proto_py_file "${generator_proto}")
        add_custom_command(
            OUTPUT ${generator_proto_py_file}
            COMMAND protoc ${protoc_FLAGS}
                --python_out=${PROJECT_BINARY_DIR}
                -I"${PROJECT_SOURCE_DIR}/generator/nanopb" ${generator_proto_file}
            DEPENDS ${generator_proto_file}
        )
        add_custom_target("generate_${generator_proto}" ALL DEPENDS ${generator_proto_py_file})
        foreach(python_instdir IN LISTS python_instdirs)
            install(
                FILES ${generator_proto_py_file}
                DESTINATION ${python_instdir}/nanopb
            )
        endforeach()
    endforeach()

    foreach(python_instdir IN LISTS python_instdirs)
        install(
            FILES generator/nanopb/__init__.py
            DESTINATION ${python_instdir}/nanopb
        )
    endforeach()

    install(
        PROGRAMS generator/nanopb_generator.py generator/protoc-gen-nanopb
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(nanopb_BUILD_RUNTIME)
    include_directories(BEFORE ${PROJECT_BINARY_DIR})

    add_library(protobuf-nanopb STATIC
        ${PROJECT_BINARY_DIR}/config.h
        pb.h
        pb_common.h
        pb_common.c
        pb_encode.h
        pb_encode.c
        pb_decode.h
        pb_decode.c)

    set_property(TARGET protobuf-nanopb PROPERTY POSITION_INDEPENDENT_CODE ON)

    configure_file(config.h.in
        config.h)

    configure_file(extra/nanopb-config-version.cmake.in
        nanopb-config-version.cmake @ONLY)

    configure_file(extra/nanopb.pc.in
        nanopb.pc @ONLY)

    install(TARGETS protobuf-nanopb EXPORT nanopb-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    install(EXPORT nanopb-targets
        DESTINATION ${CMAKE_INSTALL_CMAKEDIR}
        NAMESPACE nanopb::)

    install(FILES extra/nanopb-config.cmake
        ${PROJECT_BINARY_DIR}/nanopb-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_CMAKEDIR})

    install(FILES ${PROJECT_BINARY_DIR}/nanopb.pc
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    install(FILES ${PROJECT_BINARY_DIR}/config.h
        pb.h pb_common.h pb_encode.h pb_decode.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nanopb)

    string(REGEX REPLACE "([^;]+)" "generator/nanopb/\\1.proto" generator_proto_files "${generator_protos}")
    install(
        FILES ${generator_proto_files}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nanopb
    )
endif()
