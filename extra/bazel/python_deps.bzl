load("@rules_python//python:pip.bzl", "pip_parse")

def nanopb_python_deps(interpreter=None):
    # Required for python deps for generator plugin.
    # Used by: nanopb.
    # Used in modules: generator.
    if "nanopb_pypi" not in native.existing_rules():
        pip_parse(
            name = "nanopb_pypi",
            requirements_lock = "@nanopb//:extra/requirements_lock.txt",
            python_interpreter_target = interpreter,
        )
