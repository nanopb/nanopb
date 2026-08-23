import hashlib
from pathlib import Path
import runpy
import sys
import tempfile
import types
import unittest
from unittest.mock import patch


SCRIPT_PATH = Path(__file__).parents[2] / "generator" / "platformio_generator.py"


class FakeEnvironment:
    def __init__(self, project_dir, build_dir):
        self.project_dir = project_dir
        self.build_dir = build_dir
        self.executions = 0
        self.execute_result = 0

    def subst(self, value):
        values = {
            "$PYTHONEXE": sys.executable,
            "$PROJECT_DIR": str(self.project_dir),
            "$BUILD_DIR": str(self.build_dir),
        }
        return values.get(value, value)

    def GetProjectOption(self, name, default=None):
        values = {
            "custom_nanopb_protos": "test.proto",
            "custom_nanopb_options": "",
            "custom_nanopb_preserve_directory_hierarchy": False,
        }
        return values.get(name, default)

    def Execute(self, action):
        self.executions += 1
        return self.execute_result

    def Append(self, **kwargs):
        pass

    def BuildSources(self, build_dir, source_dir):
        pass

    def __getitem__(self, key):
        if key == "PIOENV":
            return "native"
        raise KeyError(key)


class PlatformIOGeneratorTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.project_dir = Path(self.temp_dir.name) / "project"
        self.build_dir = Path(self.temp_dir.name) / "build"
        self.project_dir.mkdir()
        self.proto_file = self.project_dir / "test.proto"
        self.options_file = self.project_dir / "test.options"
        self.proto_file.write_text('syntax = "proto3";\n')
        self.env = FakeEnvironment(self.project_dir, self.build_dir)
        self.global_env = {}

    def tearDown(self):
        self.temp_dir.cleanup()

    def run_generator(self):
        scons_action = types.ModuleType("SCons.Action")
        scons_action.CommandAction = lambda command, chdir=None: (command, chdir)
        scons = types.ModuleType("SCons")
        scons.Action = scons_action

        platformio = types.ModuleType("platformio")
        platformio.fs = types.SimpleNamespace(
            match_src_files=lambda project_dir, pattern: ["test.proto"]
        )

        grpc_tools = types.ModuleType("grpc_tools")
        grpc_tools_protoc = types.ModuleType("grpc_tools.protoc")
        grpc_tools.protoc = grpc_tools_protoc

        google = types.ModuleType("google")
        google_protobuf = types.ModuleType("google.protobuf")
        google.protobuf = google_protobuf

        modules = {
            "SCons": scons,
            "SCons.Action": scons_action,
            "platformio": platformio,
            "grpc_tools": grpc_tools,
            "grpc_tools.protoc": grpc_tools_protoc,
            "google": google,
            "google.protobuf": google_protobuf,
        }
        globals_ = {
            "env": self.env,
            "Import": lambda name: None,
            "DefaultEnvironment": lambda: self.global_env,
        }
        with patch.dict(sys.modules, modules), patch("builtins.print"):
            runpy.run_path(str(SCRIPT_PATH), init_globals=globals_)

    def test_options_cache_tracks_create_modify_and_delete(self):
        options_md5 = self.build_dir / "nanopb" / "md5" / "test.options.md5"

        self.run_generator()
        self.assertEqual(self.env.executions, 1)
        self.assertFalse(options_md5.exists())

        self.run_generator()
        self.assertEqual(self.env.executions, 1)

        self.options_file.write_text("TestMessage.value max_size:8\n")
        self.run_generator()
        self.assertEqual(self.env.executions, 2)
        self.assertEqual(
            options_md5.read_text(),
            hashlib.md5(self.options_file.read_bytes()).hexdigest(),
        )

        self.options_file.write_text("TestMessage.value max_size:16\n")
        self.run_generator()
        self.assertEqual(self.env.executions, 3)
        self.assertEqual(
            options_md5.read_text(),
            hashlib.md5(self.options_file.read_bytes()).hexdigest(),
        )

        self.options_file.unlink()
        self.run_generator()
        self.assertEqual(self.env.executions, 4)
        self.assertFalse(options_md5.exists())

        self.run_generator()
        self.assertEqual(self.env.executions, 4)

    def test_failed_regeneration_keeps_deleted_options_marker(self):
        options_md5 = self.build_dir / "nanopb" / "md5" / "test.options.md5"

        self.options_file.write_text("TestMessage.value max_size:8\n")
        self.run_generator()
        cached_md5 = options_md5.read_text()

        self.options_file.unlink()
        self.env.execute_result = 1
        with self.assertRaises(SystemExit):
            self.run_generator()

        self.assertEqual(self.env.executions, 2)
        self.assertEqual(options_md5.read_text(), cached_md5)

        self.env.execute_result = 0
        self.run_generator()
        self.assertEqual(self.env.executions, 3)
        self.assertFalse(options_md5.exists())


if __name__ == "__main__":
    unittest.main()
