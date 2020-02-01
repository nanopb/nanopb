import subprocess
import os.path

def has_grpcio_protoc():
    # type: () -> bool
    """ checks if grpcio-tools protoc is installed"""

    try:
        import grpc_tools.protoc
    except ImportError:
        return False
    return True


def invoke_protoc(argv):
    # type: (list) -> typing.Any
    """
    Invoke protoc.

    This routine will use grpcio-provided protoc if it exists,
    using system-installed protoc as a fallback.

    Args:
        argv: protoc CLI invocation, first item must be 'protoc'
    """
    if has_grpcio_protoc():
        import grpc_tools.protoc as protoc
        import pkg_resources
        proto_include = pkg_resources.resource_filename('grpc_tools', '_proto')
        nanopb_include = os.path.dirname(os.path.abspath(__file__))

        if not [x for x in argv if x.startswith('-I')]:
            argv = argv + ["-I."]

        argv = argv + ['-I' + proto_include, '-I' + nanopb_include]

        return protoc.main(argv)
    else:
        return subprocess.check_call(argv)
