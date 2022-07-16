workspace(name = "com_github_nanopb_nanopb")

load("//extra/bazel:nanopb_deps.bzl", "nanopb_deps")

nanopb_deps()

load("@rules_python//python:repositories.bzl", "python_register_toolchains")

python_register_toolchains(
    name = "python3_9",
    python_version = "3.9",
)

load("//extra/bazel:python_deps.bzl", "nanopb_python_deps")

load("@python3_9//:defs.bzl", "interpreter")

nanopb_python_deps(interpreter)

load("//extra/bazel:nanopb_workspace.bzl", "nanopb_workspace")

nanopb_workspace()
