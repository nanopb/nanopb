# Apache License, Version 2.0, January 2004, http://www.apache.org/licenses/
# Adapted from: https://github.com/rules-proto-grpc/rules_proto_grpc/

load("@rules_proto_grpc//internal:filter_files.bzl", "filter_files")
load("@rules_cc//cc:defs.bzl", "cc_library")
load(
    "@rules_proto_grpc//:defs.bzl",
    "ProtoPluginInfo",
    "proto_compile_attrs",
    "proto_compile",
)

def cc_nanopb_proto_compile_impl(ctx):
    """Nanopb proto compile implementation to add options files."""
    extra_protoc_args = getattr(ctx.attr, "extra_protoc_args", [])
    extra_protoc_files = getattr(ctx.files, "extra_protoc_files", [])
    for options_target in ctx.attr.nanopb_options_files:
        for options_file in options_target.files.to_list():
            extra_protoc_args = extra_protoc_args + [
                "--nanopb_plugin_opt=-f{}".format(options_file.path)]
            extra_protoc_files = extra_protoc_files + [options_file]
    return proto_compile(ctx, ctx.attr.options, extra_protoc_args, extra_protoc_files)


nanopb_proto_compile_attrs = dict(
    nanopb_options_files = attr.label_list(
        allow_files = [".options"],
        doc = "An optional list of additional nanopb options files to apply",
    ),
    **proto_compile_attrs,
)


# Create compile rule
cc_nanopb_proto_compile = rule(
    implementation = cc_nanopb_proto_compile_impl,
    attrs = dict(
        nanopb_proto_compile_attrs,
        _plugins = attr.label_list(
            providers = [ProtoPluginInfo],
            default = [
                Label("@nanopb//:nanopb_plugin"),
            ],
            doc = "List of protoc plugins to apply",
        ),
    ),
    toolchains = [str(Label("@rules_proto//proto:toolchain_type"))],
)


def cc_nanopb_proto_library(name, **kwargs):  # buildifier: disable=function-docstring
    # Compile protos
    name_pb = name + "_pb"
    cc_nanopb_proto_compile(
        name = name_pb,
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in nanopb_proto_compile_attrs.keys()
        }  # Forward args
    )

    # Filter files to sources and headers
    filter_files(
        name = name_pb + "_srcs",
        target = name_pb,
        extensions = ["c"],
    )

    filter_files(
        name = name_pb + "_hdrs",
        target = name_pb,
        extensions = ["h"],
    )

    # Create c library
    cc_library(
        name = name,
        srcs = [name_pb + "_srcs"],
        deps = PROTO_DEPS + kwargs.get("deps", []),
        hdrs = [name_pb + "_hdrs"],
        includes = [name_pb],
        alwayslink = kwargs.get("alwayslink"),
        copts = kwargs.get("copts"),
        defines = kwargs.get("defines"),
        features = kwargs.get("features"),
        include_prefix = kwargs.get("include_prefix"),
        linkopts = kwargs.get("linkopts"),
        linkstatic = kwargs.get("linkstatic"),
        local_defines = kwargs.get("local_defines"),
        strip_include_prefix = kwargs.get("strip_include_prefix"),
        visibility = kwargs.get("visibility"),
        tags = kwargs.get("tags"),
    )

PROTO_DEPS = [
    "@nanopb//:nanopb",
]
