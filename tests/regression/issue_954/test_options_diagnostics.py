import io
import sys
from pathlib import Path
from contextlib import redirect_stderr

def find_repo_root():
    for start in (Path(__file__).resolve(), Path.cwd().resolve()):
        for path in (start,) + tuple(start.parents):
            if (path / "generator" / "nanopb_generator.py").is_file():
                return path
    raise RuntimeError("could not find nanopb repository root")


sys.path.insert(0, str(find_repo_root()))

from generator import nanopb_generator


class NamedStringIO(io.StringIO):
    name = "invalid.options"


def expect_invalid_pattern():
    options_file = NamedStringIO("edv.GatewayMessage: type: FT_POINTER\n")
    stderr = io.StringIO()

    try:
        with redirect_stderr(stderr):
            nanopb_generator.read_options_file(options_file)
    except SystemExit as exc:
        assert exc.code == 1
    else:
        raise AssertionError("invalid options pattern was accepted")

    message = stderr.getvalue()
    assert "invalid.options:1" in message
    assert "Invalid character ':'" in message
    assert "separate the field pattern from options with whitespace" in message


def expect_file_pattern():
    options_file = NamedStringIO("dir/file-name.proto package:\"demo\"\n")
    result = nanopb_generator.read_options_file(options_file)

    assert result[0][0] == "dir/file-name.proto"


def expect_comment_markers_in_strings():
    options_file = NamedStringIO(
        "Message.url callback_datatype:\"http://example.com\" // trailing comment\n"
        "Message.tag callback_datatype:\"value#tag\" # trailing comment\n"
        "Message.block callback_datatype:\"value/*tag*/\" /* trailing comment */\n"
    )
    result = nanopb_generator.read_options_file(options_file)

    assert result[0][1].callback_datatype == "http://example.com"
    assert result[1][1].callback_datatype == "value#tag"
    assert result[2][1].callback_datatype == "value/*tag*/"


if __name__ == "__main__":
    expect_invalid_pattern()
    expect_file_pattern()
    expect_comment_markers_in_strings()
    sys.stdout.write("ok\n")
