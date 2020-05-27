load("@build_stack_rules_proto//python:python_proto_library.bzl", "python_proto_library")

proto_library(
    name = "nanopb_proto",
    srcs = [
        "nanopb.proto",
    ],
    deps = ["@com_google_protobuf//:descriptor_proto"],
)

python_proto_library(
    name = "proto_lib_python",
    transitive = True,
    deps = [":nanopb_proto"],
)

py_library(
    name = "nanopb_proto_py",
    srcs = [
        "__init__.py",
        "_utils.py",
    ],
    visibility = ["//visibility:public"],
    deps = [":proto_lib_python"],
)
