# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Test for CEL extensions."""

from typing import Any

from google.protobuf import descriptor_pool

from absl.testing import absltest
from absl.testing import parameterized
from cel_expr_python import cel
from custom_ext import sample_cel_ext
from custom_ext import sample_cel_ext_cc


class CustomExtTest(parameterized.TestCase):

  # Execute the same test for both C++ and Python implementations, which
  # are expected to produce identical results.
  EXT_IMPLEMENTATIONS = [
      ("py_ext", sample_cel_ext.SampleCelExtension()),
      ("cc_ext", sample_cel_ext_cc.SampleCelExtension()),
  ]

  def setUp(self):
    super().setUp()
    self.descriptor_pool = descriptor_pool.Default()
    self.env = cel.NewEnv(self.descriptor_pool)

  def _compile_expr(self, ext: Any, expression: str) -> cel.Expression:
    """Creates a CEL expression for the given extension and compiles the expression."""
    self.env = cel.NewEnv(
        self.descriptor_pool,
        variables={},
        extensions=[ext],
    )
    return self.env.compile(expression)

  def _create_activation(self, impl) -> cel.Activation:
    """Creates a CEL Activation with a late-bound translate function."""
    return self.env.Activation(
        {},
        functions=[
            cel.Function(
                "translate_late",
                [cel.Type.STRING],
                False,
                impl,
                return_type=cel.Type.STRING,
            )
        ],
    )

  @parameterized.named_parameters(EXT_IMPLEMENTATIONS)
  def test_basic_function(self, ext):
    compiled_expr: cel.Expression = self._compile_expr(
        ext, "'Hello, world!'.translate('en', 'es')"
    )
    act: cel.Activation = self.env.Activation({})
    res: cel.Value = compiled_expr.eval(act)

    self.assertEqual(res.value(), "¡Hola Mundo!")

  @parameterized.named_parameters(EXT_IMPLEMENTATIONS)
  def test_late_bound_function(self, ext):
    compiled_expr: cel.Expression = self._compile_expr(
        ext, "translate_late('Hello, world!')"
    )

    # Demonstrate value capture by late-binding a function.
    world: str = "Mundo"
    act: cel.Activation = self._create_activation(
        lambda _: "¡Hola {}!".format(world)
    )
    res: cel.Value = compiled_expr.eval(act)

    self.assertEqual(res.value(), "¡Hola Mundo!")

  @parameterized.named_parameters(EXT_IMPLEMENTATIONS)
  def test_error_no_matching_overload(self, ext):
    compiled_expr: cel.Expression = self._compile_expr(
        ext, "translate_late('Hello, world!')"
    )
    act: cel.Activation = self.env.Activation(
        {},
        functions=[
            cel.Function(
                "translate_late",
                [cel.Type.STRING, cel.Type.INT],
                False,
                lambda _: "¡Hola Mundo!",
                return_type=cel.Type.STRING,
            )
        ],
    )
    res: cel.Value = compiled_expr.eval(act)

    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        "No matching overloads found : translate_late(string)",
        res.value(),
    )

  @parameterized.named_parameters(EXT_IMPLEMENTATIONS)
  def test_error_wrong_arg_number(self, ext):
    compiled_expr: cel.Expression = self._compile_expr(
        ext, "translate_late('Hello, world!')"
    )
    act: cel.Activation = self._create_activation(
        _lost_in_translation_wrong_arg_number
    )
    res: cel.Value = compiled_expr.eval(act)

    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        "_lost_in_translation_wrong_arg_number() missing 1 required positional"
        " argument: 'arg2'",
        res.value(),
    )

  @parameterized.named_parameters(EXT_IMPLEMENTATIONS)
  def test_error_wrong_return_type(self, ext):
    compiled_expr: cel.Expression = self._compile_expr(
        ext, "translate_late('Hello, world!')"
    )
    act: cel.Activation = self._create_activation(
        _lost_in_translation_wrong_return_type
    )
    res: cel.Value = compiled_expr.eval(act)

    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertRegex(
        res.value(),
        r"Unexpected value type for .*_lost_in_translation_wrong_return_type.*:"
        r" int. \(Expected STRING\)",
    )

  @parameterized.named_parameters(EXT_IMPLEMENTATIONS)
  def test_error_return_none(self, ext):
    compiled_expr: cel.Expression = self._compile_expr(
        ext, "translate_late('Hello, world!')"
    )

    act: cel.Activation = self._create_activation(
        _lost_in_translation_return_none
    )

    res: cel.Value = compiled_expr.eval(act)

    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertRegex(
        res.value(),
        r"Unexpected value type for .*_lost_in_translation_return_none.*:"
        r" NoneType. \(Expected STRING\)",
    )

  @parameterized.named_parameters(EXT_IMPLEMENTATIONS)
  def test_error_propagation(self, ext):
    compiled_expr: cel.Expression = self._compile_expr(
        ext, "translate_late('Hello, world!')"
    )

    act: cel.Activation = self._create_activation(
        _lost_in_translation_raising_error
    )

    res: cel.Value = compiled_expr.eval(act)

    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn("NOT_FOUND: Lost in translation", res.value())

  def test_bad_extension_type(self):
    with self.assertRaises(Exception) as e:
      self._compile_expr(
          "Not a CelExtension", "'Hello, world!'.translate('en', 'es')"
      )
    assert "Failed to cast str either as a Python CelExtension instance" in str(
        e.exception
    )
    assert " or as a pybind11 wrapper" in str(e.exception)

  def test_none_extension(self):
    with self.assertRaises(Exception) as e:
      self._compile_expr(None, "'Hello, world!'.translate('en', 'es')")
    assert "Provided extension is None" in str(e.exception)


def _lost_in_translation_wrong_arg_number(arg1: str, arg2: str) -> str:
  return arg1 + arg2


def _lost_in_translation_wrong_return_type(arg1: str) -> int:
  return len(arg1)


def _lost_in_translation_return_none(arg1: str) -> str | None:  # pylint: disable=unused-argument
  return None


def _lost_in_translation_raising_error(text: str) -> str:  # pylint: disable=unused-argument
  raise LookupError("Lost in translation")


TEST_EXPRESSIONS = [
    ("getOrDefaultReceiver", "{'a': 1, 'b': 2}.getOrDefault('c', 3) == 3"),
    ("getOrDefault", "getOrDefault({'a': 'z', 'b': 'y'}, 'a', 'x') == 'z'"),
    ("lerp_int", "lerp(1, 2, 0.5) == 1.5"),
    ("lerp_uint", "lerp(1u, 2u, 0.5) == 1.5"),
]


class PythonTypeMappingsTest(parameterized.TestCase):

  def setUp(self):
    super().setUp()
    self.descriptor_pool = descriptor_pool.Default()
    self.env = cel.NewEnv(
        self.descriptor_pool,
        variables={},
        extensions=[sample_cel_ext.SampleCelExtension()],
    )

  @parameterized.named_parameters(TEST_EXPRESSIONS)
  def test_expression(self, expr):
    compiled_expr: cel.Expression = self.env.compile(expr)
    act: cel.Activation = self.env.Activation()
    res: cel.Value = compiled_expr.eval(act)
    self.assertEqual(res.value(), True)

  def test_lerp_error_out_of_bounds(self):
    compiled_expr: cel.Expression = self.env.compile("lerp(1, 2, 1.5)")
    act: cel.Activation = self.env.Activation()
    res: cel.Value = compiled_expr.eval(act)
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn("t must be between 0.0 and 1.0", res.value())


class OverloadExistingFunctionTest(absltest.TestCase):

  def test_overload_existing_function(self):
    env: cel.Env = cel.NewEnv(
        variables={"var_map": cel.Type.Map(cel.Type.STRING, cel.Type.DYN)},
        extensions=[
            cel.CelExtension(
                "custom_map_functions",
                functions=[
                    cel.FunctionDecl(
                        "contains",
                        [
                            cel.Overload(
                                "contains_key_value",
                                return_type=cel.Type.BOOL,
                                parameters=[
                                    cel.Type.Map(cel.Type.STRING, cel.Type.DYN),
                                    cel.Type.STRING,
                                    cel.Type.DYN,
                                ],
                                is_member=True,
                                impl=contains_key_value,
                            )
                        ],
                    )
                ],
            )
        ],
    )
    expr: cel.Expression = env.compile("var_map.contains('foo', 'bar')")

    res: cel.Value = expr.eval(data={"var_map": {"foo": "bar"}})
    self.assertTrue(res.value())
    res = expr.eval(data={"var_map": {"foo": "baz"}})
    self.assertFalse(res.value())


def contains_key_value(cel_map: dict[str, Any], key: str, value: Any) -> bool:
  return key in cel_map and cel_map[key] == value


if __name__ == "__main__":
  absltest.main()
