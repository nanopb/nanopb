import os.path
import sys
import unittest

from SCons.Script import Environment
from SCons.Tool import Tool


class NanopbToolTests(unittest.TestCase):
    def setUp(self):
        self.env = Environment(tools=[])
        self.tool = Tool("nanopb", toolpath=[toolpath])

    def test_available_tool_is_detected(self):
        self.assertTrue(self.tool.exists(self.env))

    def test_empty_protocflags_do_not_disable_tool(self):
        self.env["PROTOCFLAGS"] = ""
        self.assertTrue(self.tool.exists(self.env))


if __name__ == "__main__":
    toolpath = os.path.dirname(os.path.abspath(sys.argv.pop(1)))
    unittest.main()
