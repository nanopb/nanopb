package(default_visibility = ["//visibility:public"])

licenses(["notice"])

exports_files(["LICENSE.txt"])

cc_library(
    name = "nanopb",
    srcs = [
        "pb_common.c",
        "pb_decode.c",
        "pb_encode.c",
    ],
    hdrs = [
        "pb.h",
        "pb_common.h",
        "pb_decode.h",
        "pb_encode.h",
    ],
    strip_include_prefix = ".",
    visibility = ["//visibility:public"],
)
