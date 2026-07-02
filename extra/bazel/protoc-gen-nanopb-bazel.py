#!/usr/bin/env python3
# protoc plugin entry point for the Bazel build.
# Bazel provides nanopb_pb2 through the py_proto_library dependency, so the
# generator must not try to rebuild it inside the build action sandbox.

import os

os.environ["NANOPB_PB2_NO_REBUILD"] = "1"

from nanopb_generator import main_plugin

if __name__ == "__main__":
    main_plugin()
