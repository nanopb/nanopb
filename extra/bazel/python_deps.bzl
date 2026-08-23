# Apache License, Version 2.0, January 2004, http://www.apache.org/licenses/
"""Removed: WORKSPACE builds are no longer supported by nanopb."""

fail("nanopb no longer supports WORKSPACE builds: the nanopb Bazel rules " +
     "are now built on the @protobuf module (protobuf 35+), which requires " +
     "Bazel 8 and bzlmod. Add bazel_dep(name = \"nanopb\", ...) to your " +
     "MODULE.bazel instead. See the section \"Bazel: Bazel 8 now required, " +
     "WORKSPACE support removed\" in nanopb's docs/migration.md.")
