load("@com_github_nanopb_nanopb//rules:nanopb_proto_library.bzl", "nanopb_proto_library")

nanopb_proto_library(
    name = "example1",
    transitive = True,
    deps = ["@com_google_protobuf//:descriptor_proto"],
)

cc_library(
    name = "example2",
    hdrs = ["example2.h"],
    deps = [":example1"],
)
