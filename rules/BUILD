load("@build_stack_rules_proto//:plugin.bzl", "proto_plugin")

proto_plugin(
    name = "nanopb",
    options = [
        """-L'#include "%s"'""",
    ],
    outputs = [
        "{basename}.pb.h",
        "{basename}.pb.c",
    ],
    tool = "//generator:nanopb_generator",
    visibility = ["//visibility:public"],
)
