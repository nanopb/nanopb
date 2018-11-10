'''This file automatically rebuilds the proto definitions for Python.'''

import os
import os.path
import sys
import subprocess

dirname = os.path.dirname(__file__)
protosrc = os.path.join(dirname, "nanopb.proto")
protodst = os.path.join(dirname, "nanopb_pb2.py")

if os.path.isfile(protosrc):
    src_date = os.path.getmtime(protosrc)
    if not os.path.isfile(protodst) or os.path.getmtime(protodst) < src_date:
        cmd = ["protoc", "--python_out=.", "nanopb.proto"]
        status = subprocess.call(cmd, cwd = dirname)
        if status != 0:
            sys.stderr.write("Failed to build nanopb_pb2.py: " + ' '.join(cmd) + "\n")

