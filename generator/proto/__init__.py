'''This file dynamically builds the proto definitions for Python.'''
from __future__ import absolute_import

import os
import os.path
import sys
import tempfile
import shutil
import traceback
from tempfile import TemporaryDirectory
from ._utils import has_grpcio_protoc, invoke_protoc, print_versions

def build_nanopb_proto(protosrc, dirname):
    '''Try to build a .proto file for python-protobuf.
    Returns True if successful.
    '''

    cmd = [
        "protoc",
        "--python_out={}".format(dirname),
        protosrc,
        "-I={}".format(dirname),
    ]

    if has_grpcio_protoc():
        # grpcio-tools has an extra CLI argument
        # from grpc.tools.protoc __main__ invocation.
        cmd.append("-I={}".format(_utils.get_grpc_tools_proto_path()))

    try:
        invoke_protoc(argv=cmd)
    except:
        sys.stderr.write("Failed to build nanopb_pb2.py: " + ' '.join(cmd) + "\n")
        sys.stderr.write(traceback.format_exc() + "\n")
        return False

    return True

def load_nanopb_pb2():
    # To work, the generator needs python-protobuf built version of nanopb.proto.
    # There are three methods to provide this:
    #
    # 1) Load a previously generated generator/proto/nanopb_pb2.py
    # 2) Use protoc to build it and store it permanently generator/proto/nanopb_pb2.py
    # 3) Use protoc to build it, but store only temporarily in system-wide temp folder
    #
    # By default these are tried in numeric order.
    # If NANOPB_PB2_TEMP_DIR environment variable is defined, the 2) is skipped.
    # If the value of the $NANOPB_PB2_TEMP_DIR exists as a directory, it is used instead
    # of system temp folder.

    tmpdir = os.getenv("NANOPB_PB2_TEMP_DIR")
    temporary_only = (tmpdir is not None)
    dirname = os.path.dirname(__file__)
    protosrc = os.path.join(dirname, "nanopb.proto")
    protodst = os.path.join(dirname, "nanopb_pb2.py")

    if tmpdir is not None and not os.path.isdir(tmpdir):
        tmpdir = None # Use system-wide temp dir

    no_rebuild = bool(int(os.getenv("NANOPB_PB2_NO_REBUILD", default = 0)))
    if bool(no_rebuild):
        # Don't attempt to autogenerate nanopb_pb2.py, external build rules
        # should have already done so.
        import nanopb_pb2 as nanopb_pb2_mod
        return nanopb_pb2_mod

    if os.path.isfile(protosrc):
        src_date = os.path.getmtime(protosrc)
        if os.path.isfile(protodst) and os.path.getmtime(protodst) >= src_date:
            try:
                from . import nanopb_pb2 as nanopb_pb2_mod
                return nanopb_pb2_mod
            except Exception as e:
                sys.stderr.write("Failed to import nanopb_pb2.py: " + str(e) + "\n"
                                "Will automatically attempt to rebuild this.\n"
                                "Verify that python-protobuf and protoc versions match.\n")
                print_versions()

    # Try to rebuild into generator/proto directory
    if not temporary_only:
        build_nanopb_proto(protosrc, dirname)

        try:
            from . import nanopb_pb2 as nanopb_pb2_mod
            return nanopb_pb2_mod
        except:
            sys.stderr.write("Failed to import generator/proto/nanopb_pb2.py:\n")
            sys.stderr.write(traceback.format_exc() + "\n")

    # Try to rebuild into temporary directory
    with TemporaryDirectory(prefix = 'nanopb-', dir = tmpdir) as protodir:
        build_nanopb_proto(protosrc, protodir)

        if protodir not in sys.path:
            sys.path.insert(0, protodir)

        try:
            import nanopb_pb2 as nanopb_pb2_mod
            return nanopb_pb2_mod
        except:
            sys.stderr.write("Failed to import %s/nanopb_pb2.py:\n" % protodir)
            sys.stderr.write(traceback.format_exc() + "\n")

    # If everything fails
    sys.stderr.write("\n\nGenerating nanopb_pb2.py failed.\n")
    sys.stderr.write("Make sure that a protoc generator is available and matches python-protobuf version.\n")
    print_versions()
    sys.exit(1)

