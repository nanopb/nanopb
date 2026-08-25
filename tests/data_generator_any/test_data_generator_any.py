import sys
import ast
from pathlib import Path

from google.protobuf import any_pb2


def find_repo_root():
    for start in (Path(__file__).resolve(), Path.cwd().resolve()):
        for path in (start,) + tuple(start.parents):
            if (path / "generator" / "nanopb_data_generator.py").is_file():
                return path
    raise RuntimeError("could not find nanopb repository root")


REPO_ROOT = find_repo_root()
sys.path.insert(0, str(REPO_ROOT))

from generator.nanopb_data_generator import DataGenerator, OutputFormat


TEST_DIR = REPO_ROOT / "tests" / "data_generator_any"
PROTO_FILE = TEST_DIR / "test_messages.proto"
ALLOWED_ANY_TYPES = {
    "type.googleapis.com/anyfixture.DirectPayload",
    "type.googleapis.com/anyfixture.Host.NestedChoice",
    "type.googleapis.com/importedfixture.ImportedPayload",
}


def make_generator():
    return DataGenerator(str(PROTO_FILE), include_paths=[str(TEST_DIR)])


def any_payload_depth(data):
    depth = 0
    node = data
    while isinstance(node, dict) and "child" in node and node["child"] is not None:
        depth += 1
        node = node["child"]
    return depth


def expect_raises(error_type, fn, *args, **kwargs):
    try:
        fn(*args, **kwargs)
    except error_type:
        return
    raise AssertionError(f"Expected {error_type.__name__} to be raised")


def test_wrapper_generation_and_any_unpack():
    generator = make_generator()
    data = generator.generate_valid("Wrapper")

    assert data["direct"]["id"] == 7
    assert data["direct"]["nested"]["label"] == "nested"
    assert data["imported"]["imported_name"] == "imported"
    assert data["imported"]["nested"]["code"] == 99
    assert data["scalar"]["name"] == "scalar"
    assert data["scalar"]["count"] == 4

    any_value = data["payload"]
    assert any_value["type_url"] in ALLOWED_ANY_TYPES
    assert isinstance(any_value["value"], bytes)

    wrapper_cls = generator.registry.get_message_class("Wrapper")
    encoded = generator.encode_to_binary("Wrapper", data)
    parsed = wrapper_cls()
    parsed.ParseFromString(encoded)

    runtime_any = any_pb2.Any(type_url=parsed.payload.type_url, value=parsed.payload.value)
    payload_info = generator.registry.resolve_message(runtime_any.TypeName())
    payload_cls = generator.registry.get_message_class(payload_info.full_name)
    unpacked = payload_cls()
    assert runtime_any.Unpack(unpacked)

    if payload_info.full_name == "anyfixture.DirectPayload":
        assert unpacked.id == 7
        assert unpacked.nested.label == "nested"
    elif payload_info.full_name == "anyfixture.Host.NestedChoice":
        assert unpacked.tag == "nested-choice"
    elif payload_info.full_name == "importedfixture.ImportedPayload":
        assert unpacked.imported_name == "imported"
        assert unpacked.nested.code == 99
    else:
        raise AssertionError(payload_info.full_name)


def test_any_candidate_filtering_and_impossible_any():
    generator = make_generator()

    disallowed = generator.generate_valid("DisallowedAny", seed=3)
    assert disallowed["payload"]["type_url"] != "type.googleapis.com/anyfixture.DirectPayload"

    seen = set()
    for seed in range(30):
        seen.add(generator.generate_valid("Wrapper", seed=seed)["payload"]["type_url"])
    assert len(seen.intersection(ALLOWED_ANY_TYPES)) >= 2

    expect_raises(ValueError, generator.generate_valid, "ImpossibleAny", seed=1)


def test_reproducibility_recursion_and_dict_format():
    generator = make_generator()

    first = generator.generate_valid("Wrapper", seed=123)
    second = generator.generate_valid("Wrapper", seed=123)
    assert first == second

    recursive = generator.generate_valid("Recursive", seed=5)
    assert recursive["value"] == 11
    assert any_payload_depth(recursive) == 0

    formatted = generator.format_output(first, OutputFormat.PYTHON_DICT)
    assert ast.literal_eval(formatted) == first


if __name__ == "__main__":
    test_wrapper_generation_and_any_unpack()
    test_any_candidate_filtering_and_impossible_any()
    test_reproducibility_recursion_and_dict_format()
    sys.stdout.write("ok\n")
