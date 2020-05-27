py_binary(
    name = "nanopb_generator",
    srcs = ["nanopb_generator.py"],
    args = ["--protoc-plugin"],
    imports = [
        ".",
    ],
    main = "nanopb_generator.py",
    visibility = ["//visibility:public"],
    deps = ["//generator/proto:nanopb_proto_py"],
)
