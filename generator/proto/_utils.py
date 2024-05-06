import sys
import subprocess
import os.path

import traceback

def has_grpcio_protoc(verbose = False):
    # type: () -> bool
    """ checks if grpcio-tools protoc is installed"""

    try:
        import grpc_tools.protoc
    except ImportError:
        if verbose:
            sys.stderr.write("Failed to import grpc_tools: %s\n" % traceback.format_exc())
        return False

    return True

def get_proto_builtin_include_path():
    """Find include path for standard google/protobuf includes and for
    nanopb.proto.
    """

    if getattr(sys, 'frozen', False):
        # Pyinstaller package
        paths = [
            os.path.join(os.path.dirname(os.path.abspath(sys.executable)), 'proto'),
            os.path.join(os.path.dirname(os.path.abspath(sys.executable)), 'grpc_tools', '_proto')
        ]

    else:
        # Stand-alone script
        paths = [
            os.path.dirname(os.path.abspath(__file__))
        ]

        if has_grpcio_protoc():
            import pkg_resources
            paths.append(pkg_resources.resource_filename('grpc_tools', '_proto'))

    return paths

def invoke_protoc(argv):
    # type: (list) -> typing.Any
    """
    Invoke protoc.

    This routine will use grpcio-provided protoc if it exists,
    using system-installed protoc as a fallback.

    Args:
        argv: protoc CLI invocation, first item must be 'protoc'
    """

    # Add current directory to include path if nothing else is specified
    if not [x for x in argv if x.startswith('-I')]:
        argv.append("-I.")

    # Add default protoc include paths
    for incpath in get_proto_builtin_include_path():
        argv.append('-I' + incpath)

    if has_grpcio_protoc():
        import grpc_tools.protoc as protoc
        return protoc.main(argv)
    else:
        return subprocess.call(argv)

def print_versions():
    try:
        if has_grpcio_protoc(verbose = True):
            import grpc_tools.protoc
            sys.stderr.write("Using grpcio-tools protoc from " + grpc_tools.protoc.__file__ + "\n")
        else:
            sys.stderr.write("Using protoc from system path\n")

        invoke_protoc(['protoc', '--version'])
    except Exception as e:
        sys.stderr.write("Failed to determine protoc version: " + str(e) + "\n")

    try:
        sys.stderr.write("protoc builtin include path: " + str(get_proto_builtin_include_path()) + "\n")
    except Exception as e:
        sys.stderr.write("Failed to construct protoc include path: " + str(e) + "\n")

    try:
        import google.protobuf
        sys.stderr.write("Python version " + sys.version + "\n")
        sys.stderr.write("Using python-protobuf from " + google.protobuf.__file__ + "\n")
        sys.stderr.write("Python-protobuf version: " + google.protobuf.__version__ + "\n")
    except Exception as e:
        sys.stderr.write("Failed to determine python-protobuf version: " + str(e) + "\n")

if __name__ == '__main__':
    print_versions()
