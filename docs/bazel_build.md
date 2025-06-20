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

## Bazel Configuration Options

### Use a non-default extension

By default, nanopb generates files with extension `.pb`, which ends up creating
names such as `my_proto.pb.h` and `my_proto.pb.c`. This naming pattern conflicts
with `cc_proto_library` rules, which generate files with the same names.

To resolve this conflict, we provide a `string_flag` `//:nanopb_extension` that
currently accepts two options: `.pb` and `.nanopb`. (In the future, we could
pontentially accept your choice of input, but that was tricky to do, so to start
with only these two options are supported.)

To build `nanopb` files with the `.nanopb` extension (creating files of
`my_proto.nanopb.h` and `my_proto.nanopb.c`) you can add the following to your
command line:

```
bazel build --@nanopb//:nanopb_extension=".nanopb"
```

If you want to apply this to all nanopb files, add the above to your `.bazelrc`
file:

```
# .bazelrc
build --@nanopb//:nanopb_extension=".nanopb"
```
