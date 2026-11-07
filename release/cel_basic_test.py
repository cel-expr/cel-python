# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Basic tests for cel-python."""

import importlib.metadata
import os
import sys
import unittest

from cel_expr_python import cel
from cel_expr_python.ext import ext_bindings
from cel_expr_python.ext import ext_encoders
from cel_expr_python.ext import ext_lists
from cel_expr_python.ext import ext_math
from cel_expr_python.ext import ext_optional
from cel_expr_python.ext import ext_proto
from cel_expr_python.ext import ext_strings


class BasicTest(unittest.TestCase):

  def test_basic_compile_and_eval(self):
    env = cel.NewEnv(variables={"x": cel.Type.INT, "name": cel.Type.STRING})

    expr = env.compile("1 + 2 == 3")
    self.assertTrue(expr.eval().value())

    expr = env.compile("x * 2 + 1")
    self.assertEqual(expr.eval(data={"x": 20}).value(), 41)

    expr = env.compile("'Hello, ' + name + '!'")
    act = env.Activation({"name": "CEL"})
    self.assertEqual(expr.eval(act).value(), "Hello, CEL!")

  def test_standard_extensions(self):
    env = cel.NewEnv(
        extensions=[
            ext_bindings.ExtBindings(),
            ext_encoders.ExtEncoders(),
            ext_lists.ExtLists(),
            ext_math.ExtMath(),
            ext_optional.ExtOptional(),
            ext_proto.ExtProto(),
            ext_strings.ExtStrings(),
        ]
    )
    # Test bindings
    expr = env.compile("cel.bind(x, 2, x + 3)")
    self.assertEqual(expr.eval().value(), 5)

    # Test encoders
    expr = env.compile("base64.encode(b'hello')")
    self.assertEqual(expr.eval().value(), "aGVsbG8=")

    # Test lists
    expr = env.compile("lists.range(3)")
    self.assertEqual(expr.eval(), [0, 1, 2])
    self.assertEqual(expr.eval().value(), [0, 1, 2])
    self.assertEqual(expr.eval().plain_value(), [0, 1, 2])

    # Test math
    expr = env.compile("math.greatest(1, 5, 3)")
    self.assertEqual(expr.eval().value(), 5)

    # Test optional
    expr = env.compile("optional.of('test').orValue('fallback')")
    self.assertEqual(expr.eval().value(), "test")

    # Test strings
    expr = env.compile("strings.quote('abc')")
    self.assertEqual(expr.eval().value(), '"abc"')

  def test_custom_extension(self):
    ext = cel.CelExtension(
        "custom_ext",
        functions=[
            cel.FunctionDecl(
                "greet",
                [
                    cel.Overload(
                        "greet_string",
                        return_type=cel.Type.STRING,
                        parameters=[cel.Type.STRING],
                        is_member=False,
                        impl=lambda name: f"Hello, {name}!",
                    )
                ],
            ),
            cel.FunctionDecl(
                "greet_late",
                [
                    cel.Overload(
                        "greet_late_string",
                        return_type=cel.Type.STRING,
                        parameters=[cel.Type.STRING],
                        is_member=False,
                    )
                ],
            ),
        ],
    )
    env = cel.NewEnv(extensions=[ext])
    expr = env.compile("greet('world') + ' ' + greet_late('CEL')")
    act = env.Activation(
        {},
        functions=[
            cel.Function(
                "greet_late",
                [cel.Type.STRING],
                False,
                lambda name: f"Hi, {name}!",
                return_type=cel.Type.STRING,
            )
        ],
    )
    res = expr.eval(act)
    self.assertEqual(res.value(), "Hello, world! Hi, CEL!")

  def test_wheel_package_contents(self):
    # Exclude the script's own directory (which may be the build directory
    # containing a source `.egg-info`) so we inspect the installed wheel's
    # `.dist-info` distribution in site-packages.
    script_dir = os.path.realpath(os.path.dirname(os.path.abspath(__file__)))
    search_paths = [
        p for p in sys.path if p and os.path.realpath(p) != script_dir
    ]
    dists = list(
        importlib.metadata.distributions(
            name="cel-expr-python", path=search_paths
        )
    )
    self.assertTrue(
        dists, "Could not find installed cel-expr-python distribution"
    )
    dist = dists[0]
    top_level = dist.read_text("top_level.txt")
    if top_level is not None:
      self.assertEqual(top_level.strip().splitlines(), ["cel_expr_python"])

    self.assertIsNotNone(dist.files)
    files = [f.as_posix().replace("\\", "/") for f in dist.files]

    # Verify all expected .pyi stubs are packaged
    expected_stubs = [
        "cel_expr_python/cel.pyi",
        "cel_expr_python/ext/ext_bindings.pyi",
        "cel_expr_python/ext/ext_encoders.pyi",
        "cel_expr_python/ext/ext_lists.pyi",
        "cel_expr_python/ext/ext_math.pyi",
        "cel_expr_python/ext/ext_optional.pyi",
        "cel_expr_python/ext/ext_proto.pyi",
        "cel_expr_python/ext/ext_strings.pyi",
    ]
    for stub in expected_stubs:
      self.assertIn(stub, files)

    # Verify all compiled C++ extension modules are packaged
    expected_modules = [
        "cel_expr_python/cel.",
        "cel_expr_python/ext/ext_bindings.",
        "cel_expr_python/ext/ext_encoders.",
        "cel_expr_python/ext/ext_lists.",
        "cel_expr_python/ext/ext_math.",
        "cel_expr_python/ext/ext_optional.",
        "cel_expr_python/ext/ext_proto.",
        "cel_expr_python/ext/ext_strings.",
    ]
    binary_suffixes = (".so", ".pyd", ".dylib")
    for module_prefix in expected_modules:
      found = any(
          f.startswith(module_prefix) and f.endswith(binary_suffixes)
          for f in files
      )
      self.assertTrue(found, f"Missing binary module for {module_prefix}")

    unexpected = [
        f
        for f in files
        if not f.startswith(
            ("cel_expr_python/", "cel_expr_python-", "cel_expr_python.libs/")
        )
        or f.endswith("_test.py")
        or "/testdata/" in f
    ]
    self.assertEqual(
        unexpected,
        [],
        f"Unexpected files packaged in cel-expr-python wheel:"
        f" {unexpected[:20]}",
    )


if __name__ == "__main__":
  unittest.main()
