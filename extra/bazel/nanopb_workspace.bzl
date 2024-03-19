load("@rules_proto_grpc//:repositories.bzl", "rules_proto_grpc_repos", "rules_proto_grpc_toolchains")

def nanopb_workspace():
    rules_proto_grpc_repos()
    rules_proto_grpc_toolchains()
