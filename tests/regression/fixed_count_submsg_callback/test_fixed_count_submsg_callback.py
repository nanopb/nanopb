# Regression test for the fixed_count + submsg_callback combination.
#
# submsg_callback stores a pb_callback_t in front of the field's _count
# (repeated) or which_ (oneof) member. A fixed count array has neither, so
# the generator used to still mark the field MSG_W_CB while emitting no
# callback slot. pb_dec_submessage()/pb_enc_submessage() then read the
# callback from (pb_callback_t*)field->pSize - 1, which points outside the
# message struct. The generator must reject the combination instead.

import sys
from pathlib import Path


def find_repo_root():
    for start in (Path(__file__).resolve(), Path.cwd().resolve()):
        for path in (start,) + tuple(start.parents):
            if (path / "generator" / "nanopb_generator.py").is_file():
                return path
    raise RuntimeError("could not find nanopb repository root")


sys.path.insert(0, str(find_repo_root()))

from generator import nanopb_generator
from generator.proto import nanopb_pb2

FieldD = nanopb_generator.FieldD


def message_field():
    desc = nanopb_generator.descriptor.FieldDescriptorProto()
    desc.name = "items"
    desc.number = 1
    desc.label = FieldD.LABEL_REPEATED
    desc.type = FieldD.TYPE_MESSAGE
    desc.type_name = ".Sub"
    return desc


def options(**kwargs):
    opts = nanopb_pb2.NanoPBOptions()
    for key, value in kwargs.items():
        setattr(opts, key, value)
    return opts


def expect_fixed_count_rejected():
    try:
        nanopb_generator.Field("Msg", message_field(),
                               options(max_count=2, fixed_count=True, submsg_callback=True))
    except Exception as exc:
        assert "fixed_count" in str(exc)
        assert "submsg_callback" in str(exc)
    else:
        raise AssertionError("fixed_count + submsg_callback was accepted")


def expect_repeated_still_works():
    field = nanopb_generator.Field("Msg", message_field(),
                                   options(max_count=2, submsg_callback=True))
    assert field.pbtype == "MSG_W_CB"
    assert field.rules == "REPEATED"


if __name__ == "__main__":
    expect_fixed_count_rejected()
    expect_repeated_still_works()
    sys.stdout.write("ok\n")
