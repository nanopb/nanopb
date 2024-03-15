load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def nanopb_deps():
    # Setup proto rules.
    # Used by: nanopb, rules_proto_grpc.
    # Used in modules: root.
    if "rules_proto" not in native.existing_rules():
        http_archive(
            name = "rules_proto",
            sha256 = "dc3fb206a2cb3441b485eb1e423165b231235a1ea9b031b4433cf7bc1fa460dd",
            strip_prefix = "rules_proto-5.3.0-21.7",
            urls = [
                "https://github.com/bazelbuild/rules_proto/archive/refs/tags/5.3.0-21.7.tar.gz",
            ],
        )

    # Setup grpc tools.
    # Used by: nanopb.
    # Used in modules: generator/proto.
    if "rules_proto_grpc" not in native.existing_rules():
        http_archive(
            name = "rules_proto_grpc",
            sha256 = "c0d718f4d892c524025504e67a5bfe83360b3a982e654bc71fed7514eb8ac8ad",
            strip_prefix = "rules_proto_grpc-4.6.0",
            urls = ["https://github.com/rules-proto-grpc/rules_proto_grpc/archive/4.6.0.tar.gz"],
        )
