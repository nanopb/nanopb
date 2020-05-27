load("@build_stack_rules_proto//:compile.bzl", "proto_compile")

def nanopb_proto_compile(**kwargs):
    proto_compile(
        plugins = [
            str(Label("@com_github_nanopb_nanopb//rules:nanopb")),
        ],
        **kwargs
    )
