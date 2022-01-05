load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def nanopb_deps():
    # Setup proto rules.
    # Used by: com_github_nanopb_nanopb, rules_proto_grpc.
    # Used in modules: root.
    if "rules_proto" not in native.existing_rules():
        http_archive(
            name = "rules_proto",
            sha256 = "66bfdf8782796239d3875d37e7de19b1d94301e8972b3cbd2446b332429b4df1",
            strip_prefix = "rules_proto-4.0.0",
            urls = [
                "https://mirror.bazel.build/github.com/bazelbuild/rules_proto/archive/refs/tags/4.0.0.tar.gz",
                "https://github.com/bazelbuild/rules_proto/archive/refs/tags/4.0.0.tar.gz",
            ],
        )

    # Required for plugin rules.
    # Used by: com_github_nanopb_nanopb.
    # Used in modules: generator.
    if "rules_python" not in native.existing_rules():
        http_archive(
            name = "rules_python",
            sha256 = "5fa3c738d33acca3b97622a13a741129f67ef43f5fdfcec63b29374cc0574c29",
            strip_prefix = "rules_python-0.9.0",
            url = "https://github.com/bazelbuild/rules_python/archive/refs/tags/0.9.0.tar.gz",
        )

    # Setup grpc tools.
    # Used by: com_github_nanopb_nanopb.
    # Used in modules: generator/proto.
    if "rules_proto_grpc" not in native.existing_rules():
        http_archive(
            name = "rules_proto_grpc",
            sha256 = "507e38c8d95c7efa4f3b1c0595a8e8f139c885cb41a76cab7e20e4e67ae87731",
            strip_prefix = "rules_proto_grpc-4.1.1",
            urls = ["https://github.com/rules-proto-grpc/rules_proto_grpc/archive/4.1.1.tar.gz"],
        )
