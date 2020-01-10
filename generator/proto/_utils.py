import subprocess

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
        return protoc.main(argv)
    else:
        return subprocess.check_call(argv)
