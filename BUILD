licenses(["notice"])

exports_files(["LICENSE.txt"])

package(default_visibility = ["//visibility:public"])

cc_library(
  name = "nanopb",
  visibility = ["//visibility:public"],
  hdrs = [
    "src/pb.h",
    "src/pb_common.h",
    "src/pb_decode.h",
    "src/pb_encode.h",
  ],
  srcs = [
    "src/pb_common.c",
    "src/pb_decode.c",
    "src/pb_encode.c",
  ],
)
