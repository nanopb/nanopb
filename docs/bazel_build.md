# Nanopb: Bazel build
The Bazel build system, is designed to be fast and correct. Nanopb provides a
set of plugins for the Bazel build system allowing Nanopb to be integrated 
into the build.

## Getting started
Add the following to your MODULE.bazel file.
``` py 
# MODULE.bazel
bazel_dep(name = "nanopb", version = "0.4.9")
git_override(
    module_name = "nanopb",
    remote = "https://github.com/nanopb/nanopb.git",
    commit = "<commit>",
)
```

Or if you would prefer using WORKSPACE file
```py
git_repository(
    name = "com_github_nanopb_nanopb",
    remote = "https://github.com/nanopb/nanopb.git"
    commit = "<TODO:Enter your desired commit>",
)

load("@nanopb//extra/bazel:nanopb_deps.bzl", "nanopb_deps")

nanopb_deps()

load("@nanopb//extra/bazel:python_deps.bzl", "nanopb_python_deps")

nanopb_python_deps()

load("@nanopb//extra/bazel:nanopb_workspace.bzl", "nanopb_workspace")

nanopb_workspace()
```

To use the Nanopb rules with in your build you can use the 
`cc_nanopb_proto_library` which works in a similar way to the native
`cc_proto_library` rule.
```  py
# BUILD.bazel
load("@nanopb//extra/bazel:nanopb_cc_proto_library.bzl", "cc_nanopb_proto_library")

# Your native proto_library.
proto_library(
    name = "descriptor",
    srcs = [
        "generator/proto/google/protobuf/descriptor.proto",
    ],
)

# Generated library.
cc_nanopb_proto_library(
    name = "descriptor_nanopb",
    protos = [":descriptor"],
    visibility = ["//visibility:private"],
)

# Depend directly on the generated code using a cc_library.
cc_library(
    name = "uses_generated_descriptors",
    deps = [":descriptor_nanopb"],
    hdrs = ["my_header.h"],
)
```

If you have a custom nanopb options file, use the `nanopb_options_files` argument shown below.
```  py
# Generated library with options.
cc_nanopb_proto_library(
    name = "descriptor_nanopb",
    protos = [":descriptor"],
    nanopb_options_files = ["descriptor.options"],
    visibility = ["//visibility:private"],
)
```
