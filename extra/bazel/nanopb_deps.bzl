load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def nanopb_deps():
    # Setup grpc tools.
    # Used by: nanopb.
    # Used in modules: generator/proto.
    if "rules_proto_grpc" not in native.existing_rules():
        http_archive(
            name = "rules_proto_grpc",
            sha256 = "c0d718f4d892c524025504e67a5bfe83360b3a982e654bc71fed7514eb8ac8ad",
            strip_prefix = "rules_proto_grpc-5.0.0-alpha2",
            urls = ["https://github.com/rules-proto-grpc/rules_proto_grpc/releases/download/5.0.0-alpha2/rules_proto_grpc-5.0.0-alpha2.tar.gz"],
        )

    # Required for plugin rules.
    # Used by: nanopb.
    # Used in modules: generator.
    if "rules_python" not in native.existing_rules():
        http_archive(
            name = "rules_python",
            sha256 = "c68bdc4fbec25de5b5493b8819cfc877c4ea299c0dcb15c244c5a00208cde311",
            strip_prefix = "rules_python-0.31.0",
            url = "https://github.com/bazelbuild/rules_python/releases/download/0.31.0/rules_python-0.31.0.tar.gz",
        )

    # Required for rule `copy_file`.
    # Used by: nanopb.
    # Used in modules: generator.
    if "bazel_skylib" not in native.existing_rules():
        http_archive(
            name = "bazel_skylib",
            sha256 = "cd55a062e763b9349921f0f5db8c3933288dc8ba4f76dd9416aac68acee3cb94",
            urls = [
                "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.5.0/bazel-skylib-1.5.0.tar.gz",
                "https://github.com/bazelbuild/bazel-skylib/releases/download/1.5.0/bazel-skylib-1.5.0.tar.gz",
            ],
        )
