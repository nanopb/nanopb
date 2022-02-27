'''This file automatically rebuilds the proto definitions for Python.'''
from __future__ import absolute_import

import os.path
import sys

import pkg_resources

from ._utils import has_grpcio_protoc, invoke_protoc, print_versions

dirname = os.path.dirname(__file__)
protosrc = os.path.join(dirname, "nanopb.proto")
protodst = os.path.join(dirname, "nanopb_pb2.py")
rebuild = False

if os.path.isfile(protosrc):
    src_date = os.path.getmtime(protosrc)
    if not os.path.isfile(protodst) or os.path.getmtime(protodst) < src_date:
        rebuild = True

if not rebuild:
    try:
        from . import nanopb_pb2
    except AttributeError as e:
        rebuild = True
        sys.stderr.write("Failed to import nanopb_pb2.py: " + str(e) + "\n"
                         "Will automatically attempt to rebuild this.\n"
                         "Verify that python-protobuf and protoc versions match.\n")
        print_versions()

if rebuild:
    cmd = [
        "protoc",
        "--python_out={}".format(dirname),
        protosrc,
        "-I={}".format(dirname),
    ]

    if has_grpcio_protoc():
        # grpcio-tools has an extra CLI argument
        # from grpc.tools.protoc __main__ invocation.
        _builtin_proto_include = pkg_resources.resource_filename('grpc_tools', '_proto')

        cmd.append("-I={}".format(_builtin_proto_include))
    try:
        invoke_protoc(argv=cmd)
    except:
        sys.stderr.write("Failed to build nanopb_pb2.py: " + ' '.join(cmd) + "\n")
        raise

