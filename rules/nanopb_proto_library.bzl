load("//rules:nanopb_proto_compile.bzl", "nanopb_proto_compile")

def nanopb_proto_library(**kwargs):
    name = kwargs.get("name")
    deps = kwargs.get("deps")
    visibility = kwargs.get("visibility")

    name_pb = name + "_pb"
    nanopb_proto_compile(
        name = name_pb,
        deps = deps,
        visibility = visibility,
        verbose = kwargs.pop("verbose", 0),
        transitivity = kwargs.pop("transitivity", {}),
        transitive = kwargs.pop("transitive", True),
    )

    native.cc_library(
        name = name,
        srcs = [name_pb],
        deps = [
            "@com_github_nanopb_nanopb//:nanopb",
        ],
        includes = [name_pb],
        visibility = visibility,
    )
