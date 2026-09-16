# Verify that the generator refuses enum values that do not fit in the
# integer size selected with the enum_intsize option. Without this check
# the values would silently truncate in the fallback branch of the
# generated header, while the 'enum : type' branch would fail to compile.

import sys
from pathlib import Path


def find_repo_root():
    for start in (Path(__file__).resolve(), Path.cwd().resolve()):
        for path in (start,) + tuple(start.parents):
            if (path / "generator" / "nanopb_generator.py").is_file():
                return path
    raise RuntimeError("could not find nanopb repository root")


sys.path.insert(0, str(find_repo_root()))

from google.protobuf import descriptor_pb2

from generator import nanopb_generator
from generator.proto import nanopb_pb2


def make_enum(intsize, values):
    desc = descriptor_pb2.EnumDescriptorProto()
    desc.name = "TestEnum"
    for name, number in values:
        value = desc.value.add()
        value.name = name
        value.number = number

    options = nanopb_pb2.NanoPBOptions()
    options.enum_intsize = intsize
    options.long_names = True

    return nanopb_generator.Enum(nanopb_generator.Names(["TestEnum"]), desc, options, (), {})


def expect_rejected(intsize, values, description, expect_in_message):
    try:
        str(make_enum(intsize, values))
    except Exception as e:
        if expect_in_message not in str(e):
            print("[FAIL] rejected %s, but the message does not mention '%s': %s"
                  % (description, expect_in_message, e))
            return 1

        print("[ OK ] rejected %s: %s" % (description, e))
        return 0

    print("[FAIL] accepted %s" % description)
    return 1


def expect_accepted(intsize, values, description):
    try:
        str(make_enum(intsize, values))
    except Exception as e:
        print("[FAIL] rejected %s: %s" % (description, e))
        return 1

    print("[ OK ] accepted %s" % description)
    return 0


status = 0
status += expect_accepted(nanopb_pb2.IS_8, [("A", 0), ("B", 255)], "largest value that fits in IS_8")
status += expect_accepted(nanopb_pb2.IS_16, [("A", 0), ("B", 65535)], "largest value that fits in IS_16")
status += expect_rejected(nanopb_pb2.IS_8, [("A", 0), ("B", 256)],
                          "value one above IS_8 range", "Use a larger enum_intsize")
status += expect_rejected(nanopb_pb2.IS_16, [("A", 0), ("B", 70000)],
                          "value above IS_16 range", "Use a larger enum_intsize")

# A larger size does not help for negative values, so the message must not
# suggest one.
status += expect_rejected(nanopb_pb2.IS_8, [("A", -1), ("B", 1)],
                          "negative value", "is negative")

if status != 0:
    print("\n\nSome tests FAILED!")

sys.exit(status)
