load("@nanopb_pypi//:requirements.bzl", "install_deps")
load("@rules_proto_grpc//:repositories.bzl", "rules_proto_grpc_repos", "rules_proto_grpc_toolchains")
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")

def nanopb_workspace():
    install_deps()
    rules_proto_grpc_toolchains()
    rules_proto_grpc_repos()
    rules_proto_dependencies()
    rules_proto_toolchains()
