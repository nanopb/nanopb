load("@nanopb//extra/bazel:nanopb_cc_proto_library.bzl", "cc_nanopb_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "test_proto",
    srcs = ["test.proto"],
)

cc_nanopb_proto_library(
    name = "test_proto_nanopb",
    protos = [":test_proto"],
    nanopb_options_files = ["test.options"],
)
