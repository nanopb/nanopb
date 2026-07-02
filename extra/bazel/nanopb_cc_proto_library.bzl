# Apache License, Version 2.0, January 2004, http://www.apache.org/licenses/
"""Rules for generating nanopb C libraries from proto_library targets.

Implemented directly on the proto rules shipped with @protobuf, which is
the upstream home of Bazel proto support. Generated files are placed in a
directory named after the compile rule, mirroring each .proto file's
import path, e.g. `<name>_pb/google/protobuf/descriptor.pb.h`.
"""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("@protobuf//bazel/common:proto_common.bzl", "proto_common")
load("@protobuf//bazel/common:proto_info.bzl", "ProtoInfo")
load("@rules_cc//cc:defs.bzl", "cc_library")

NANOPB_TOOLCHAIN_TYPE = Label("//extra/bazel:toolchain_type")

# Mirrors @protobuf//bazel/private:toolchain_helpers.bzl, whose header asks
# other repositories to copy it while the proto toolchain migration is in
# progress. Remove once the incompatible flag is flipped and removed.
_TOOLCHAIN_RESOLUTION_ENABLED = proto_common.INCOMPATIBLE_ENABLE_PROTO_TOOLCHAIN_RESOLUTION

def _find_toolchain(ctx):
    if _TOOLCHAIN_RESOLUTION_ENABLED:
        toolchain = ctx.toolchains[NANOPB_TOOLCHAIN_TYPE]
        if not toolchain:
            fail("No toolchains registered for '%s'." % NANOPB_TOOLCHAIN_TYPE)
        return toolchain.proto
    return ctx.attr._toolchain[proto_common.ProtoLangToolchainInfo]

def _use_toolchain():
    if _TOOLCHAIN_RESOLUTION_ENABLED:
        return [config_common.toolchain_type(NANOPB_TOOLCHAIN_TYPE, mandatory = False)]
    return []

def _if_legacy_toolchain(legacy_attrs):
    if _TOOLCHAIN_RESOLUTION_ENABLED:
        return {}
    return legacy_attrs

def _output_stem(proto_source):
    """Returns a source's output path stem, relative to the output directory."""
    import_path = proto_common.get_import_path(proto_source)
    return import_path[:-(len(proto_source.extension) + 1)]

def _declare_outputs(ctx, proto_info, extension):
    # Deliberately not proto_common.declare_generated_files(): that declares
    # one canonical output path next to each .proto, which conflicts when two
    # targets generate the same proto with different options files, and puts
    # external-repo outputs where cc_library includes cannot reach them.
    headers = []
    sources = []
    for src in proto_info.direct_sources:
        stem = _output_stem(src)
        headers.append(ctx.actions.declare_file("{}/{}{}.h".format(ctx.label.name, stem, extension)))
        sources.append(ctx.actions.declare_file("{}/{}{}.c".format(ctx.label.name, stem, extension)))
    return headers, sources

def _output_directory(ctx):
    """Returns the execroot-relative directory protoc should generate into."""
    components = [ctx.bin_dir.path]
    if ctx.label.workspace_root:
        components.append(ctx.label.workspace_root)
    if ctx.label.package:
        components.append(ctx.label.package)
    components.append(ctx.label.name)
    return "/".join(components)

def _plugin_args(ctx, extension):
    args = ctx.actions.args()
    args.add("--nanopb_opt=--library-include-format=quote")
    args.add("--nanopb_opt=--extension={}".format(extension))
    args.add_all(ctx.files.nanopb_options_files, format_each = "--nanopb_opt=-f%s")
    args.add_all(ctx.attr.extra_protoc_args)
    return args

def _cc_nanopb_proto_compile_impl(ctx):
    extension = ctx.attr._extension[BuildSettingInfo].value
    toolchain = _find_toolchain(ctx)
    extra_inputs = ctx.files.nanopb_options_files + ctx.files.extra_protoc_files

    headers = []
    sources = []
    for proto in ctx.attr.protos:
        proto_info = proto[ProtoInfo]
        proto_headers, proto_sources = _declare_outputs(ctx, proto_info, extension)
        proto_common.compile(
            actions = ctx.actions,
            proto_info = proto_info,
            proto_lang_toolchain_info = toolchain,
            generated_files = proto_headers + proto_sources,
            plugin_output = _output_directory(ctx),
            additional_args = _plugin_args(ctx, extension),
            additional_inputs = depset(extra_inputs),
        )
        headers += proto_headers
        sources += proto_sources

    return [
        DefaultInfo(files = depset(headers + sources)),
        OutputGroupInfo(
            hdrs = depset(headers),
            srcs = depset(sources),
        ),
    ]

cc_nanopb_proto_compile = rule(
    implementation = _cc_nanopb_proto_compile_impl,
    doc = "Generates nanopb .c/.h sources for the given proto_library targets.",
    attrs = {
        "protos": attr.label_list(
            providers = [ProtoInfo],
            mandatory = True,
            doc = "proto_library targets to generate nanopb sources for",
        ),
        "nanopb_options_files": attr.label_list(
            allow_files = [".options"],
            doc = "An optional list of additional nanopb options files to apply",
        ),
        "extra_protoc_args": attr.string_list(
            doc = "Additional command line arguments to pass to protoc",
        ),
        "extra_protoc_files": attr.label_list(
            allow_files = True,
            doc = "Additional files to make available to protoc",
        ),
        "_extension": attr.label(
            doc = "Private field to allow //:nanopb_extension string_flag to apply",
            default = "//:nanopb_extension",
        ),
    } | _if_legacy_toolchain({
        "_toolchain": attr.label(
            default = "//:nanopb_proto_toolchain",
            providers = [proto_common.ProtoLangToolchainInfo],
        ),
    }),
    toolchains = _use_toolchain(),
)

_COMPILE_ATTR_NAMES = [
    "protos",
    "nanopb_options_files",
    "extra_protoc_args",
    "extra_protoc_files",
]

def cc_nanopb_proto_library(name, **kwargs):
    """Generates a cc_library from proto_library targets using the nanopb plugin.

    Args accepted by cc_nanopb_proto_compile are forwarded to it; all other
    keyword args are forwarded to the underlying cc_library.
    """
    name_pb = name + "_pb"
    compile_kwargs = {
        attr: kwargs.pop(attr)
        for attr in _COMPILE_ATTR_NAMES
        if attr in kwargs
    }
    cc_nanopb_proto_compile(
        name = name_pb,
        **compile_kwargs
    )

    # Filter files to sources and headers
    native.filegroup(
        name = name_pb + "_srcs",
        srcs = [name_pb],
        output_group = "srcs",
    )

    native.filegroup(
        name = name_pb + "_hdrs",
        srcs = [name_pb],
        output_group = "hdrs",
    )

    cc_library(
        name = name,
        srcs = [name_pb + "_srcs"],
        deps = PROTO_DEPS + kwargs.pop("deps", []),
        hdrs = [name_pb + "_hdrs"],
        includes = [name_pb],
        **kwargs
    )

PROTO_DEPS = [
    Label("//:nanopb"),
]
