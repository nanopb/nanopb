import subprocess


def has_system_protoc():
    # type: () -> bool
    """ checks if a system-installed `protoc` executable exists """

    try:
        subprocess.check_call("protoc --version".split())
    except FileNotFoundError:
        return False
    return True


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
        argv: protoc CLI invocation
    """
    if has_grpcio_protoc():
        import grpc_tools.protoc as protoc
        return protoc.main(argv)
    if has_system_protoc():
        return subprocess.check_call(argv)

    raise FileNotFoundError("Neither grpc-tools nor system provided protoc could be found.")
