'''This file automatically rebuilds the proto definitions for Python.'''

import os.path
import sys

from grpc_tools import protoc
import pkg_resources

dirname = os.path.dirname(__file__)
protosrc = os.path.join(dirname, "nanopb.proto")
protodst = os.path.join(dirname, "nanopb_pb2.py")

if os.path.isfile(protosrc):
    src_date = os.path.getmtime(protosrc)
    if not os.path.isfile(protodst) or os.path.getmtime(protodst) < src_date:

        # from grpc.tools.protoc __main__ invocation
        _builtin_proto_include = pkg_resources.resource_filename('grpc_tools', '_proto')

        cmd = [
            "protoc",
            "--python_out={}".format(dirname),
            protosrc, "-I={}".format(dirname),
            "-I={}".format(_builtin_proto_include)
        ]
        try:
            protoc.main(cmd)
        except:
            sys.stderr.write("Failed to build nanopb_pb2.py: " + ' '.join(cmd) + "\n")
            raise
