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

"""Tests for Env/Config.

This module contains tests for the `cel.EnvConfig` class, focusing on its
ability to be created from and serialized to YAML format.
"""

import textwrap
from typing import Any

from absl.testing import absltest
from cel_expr_python import cel
from cel_expr_python.ext import ext_bindings
from cel_expr_python.ext import ext_math
from cel_expr_python.ext import ext_optional
from cel_expr_python.ext import ext_strings
from cel.expr.conformance.proto2 import test_all_types_pb2 as test_all_types_pb


class CelEnvTest(absltest.TestCase):

  def test_env_config_from_and_to_yaml(self):
    config: cel.EnvConfig = cel.NewEnvConfigFromYaml("""
      name: foo
      container: test.container
      stdlib:
        exclude_macros:
        - map
        - filter
        exclude_functions:
        - name: "_+_"
      extensions:
        - name: math
      variables:
      - name: one
        type: int
        value: 1
      functions:
        - name: add
          overloads:
            - signature: "add(int,int)"
              return: int
    """)
    yaml: str = config.to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
      name: "foo"
      container: "test.container"
      extensions:
        - name: "math"
      stdlib:
        exclude_macros:
          - "filter"
          - "map"
        exclude_functions:
          - name: "_+_"
      variables:
        - name: "one"
          type: "int"
          value: 1
      functions:
        - name: "add"
          overloads:
            - signature: "add(int,int)"
              return: "int"
    """),
    )

  def test_invalid_yaml(self):
    with self.assertRaises(Exception) as e:
      cel.NewEnvConfigFromYaml(" invalid yaml")
    self.assertIn(
        "1:2: Invalid CEL environment config YAML\n"
        + "| invalid yaml\n"
        + "| ^",
        str(e.exception),
    )

  def test_parse_context_variable_config(self):
    config = cel.NewEnvConfigFromYaml("""
      context_variable:
        type_name: "cel.expr.conformance.proto2.TestAllTypes"
    """)
    self.assertEqual(
        config.context_type, "cel.expr.conformance.proto2.TestAllTypes"
    )

  def test_parse_context_variable_config_alternative_syntax(self):
    config = cel.NewEnvConfigFromYaml("""
      context_variable:
        type: "cel.expr.conformance.proto2.TestAllTypes"
    """)
    self.assertEqual(
        config.context_type, "cel.expr.conformance.proto2.TestAllTypes"
    )

  def test_parse_context_variable_malformed(self):
    with self.assertRaisesRegex(
        Exception, "Node 'context_variable' is not a map"
    ):
      cel.NewEnvConfigFromYaml("context_variable: 123")

  def test_parse_context_variable_malformed2(self):
    with self.assertRaisesRegex(
        Exception, "Node 'context_variable' does not have a valid type"
    ):
      cel.NewEnvConfigFromYaml("""
        context_variable:
          type:
            foo: bar
      """)

  def test_context_variable_basic(self):
    config = cel.NewEnvConfigFromYaml("""
      context_variable:
        type_name: "cel.expr.conformance.proto2.TestAllTypes"
    """)
    env = cel.NewEnv(config=config)
    ast = env.compile("single_int32 > 10")
    self.assertIsNotNone(ast)

    with self.assertRaises(Exception):
      env.compile("non_existent_field > 10")

  def test_config_export_container(self):
    env: cel.Env = cel.NewEnv(container="test.container")
    yaml: str = env.config().to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
          container: "test.container"
        """),
    )

  def test_expression_container_abbreviations_and_aliases(self):
    expr_container = cel.ExpressionContainer(
        "test.container", abbreviations=["x.y.foo"], aliases={"abc": "x.y.bar"}
    )

    env: cel.Env = cel.NewEnv(
        container=expr_container,
        variables={
            "x.y.foo": cel.Type.INT,
            "x.y.bar": cel.Type.STRING,
        },
    )

    res = env.compile("foo").eval(data={"x.y.foo": 42})
    self.assertEqual(res.value(), 42)
    res = env.compile("abc").eval(data={"x.y.bar": "chocolate"})
    self.assertEqual(res.value(), "chocolate")

    yaml: str = env.config().to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
          container:
            name: "test.container"
            abbreviations:
              - "x.y.foo"
            aliases:
              - alias: "abc"
                qualified_name: "x.y.bar"
          variables:
            - name: "x.y.bar"
              type: "string"
            - name: "x.y.foo"
              type: "int"
        """),
    )

  def test_abbreviations_and_aliases_from_yaml(self):
    env: cel.Env = cel.NewEnv(config=cel.NewEnvConfigFromYaml("""
        container:
          name: "test.container"
          abbreviations:
            - "x.y.foo"
          aliases:
            - alias: "abc"
              qualified_name: "x.y.bar"
        variables:
          - name: "x.y.bar"
            type: "string"
          - name: "x.y.foo"
            type: "int"
      """))

    res = env.compile("foo").eval(data={"x.y.foo": 42})
    self.assertEqual(res.value(), 42)
    res = env.compile("abc").eval(data={"x.y.bar": "chocolate"})
    self.assertEqual(res.value(), "chocolate")

  def test_abbreviations_and_aliases_combined(self):
    env: cel.Env = cel.NewEnv(
        config=cel.NewEnvConfigFromYaml("""
          container:
            name: "test.container"
            abbreviations:
              - "x.y.foo"
            aliases:
              - alias: "abc"
                qualified_name: "x.y.bar"
          variables:
            - name: "x.y.bar"
              type: "string"
            - name: "x.y.foo"
              type: "int"
            - name: "a.b.qux"
              type: "string"
            - name: "a.b.baz"
              type: "int"
        """),
        container=cel.ExpressionContainer(
            "test.container",
            abbreviations=["a.b.baz"],
            aliases={"def": "a.b.qux"},
        ),
    )

    res = env.compile("foo").eval(data={"x.y.foo": 42})
    self.assertEqual(res.value(), 42)
    res = env.compile("baz").eval(data={"a.b.baz": 24})
    self.assertEqual(res.value(), 24)

    res = env.compile("abc").eval(data={"x.y.bar": "chocolate"})
    self.assertEqual(res.value(), "chocolate")
    res = env.compile("def").eval(data={"a.b.qux": "vanilla"})
    self.assertEqual(res.value(), "vanilla")

    yaml: str = env.config().to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
          container:
            name: "test.container"
            abbreviations:
              - "a.b.baz"
              - "x.y.foo"
            aliases:
              - alias: "abc"
                qualified_name: "x.y.bar"
              - alias: "def"
                qualified_name: "a.b.qux"
          variables:
            - name: "a.b.baz"
              type: "int"
            - name: "a.b.qux"
              type: "string"
            - name: "x.y.bar"
              type: "string"
            - name: "x.y.foo"
              type: "int"
        """),
    )

  def test_alias_redefinition_error(self):
    with self.assertRaises(Exception) as e:
      cel.NewEnv(
          container=cel.ExpressionContainer(
              "test.container", aliases={"abc": "x.y.bar"}
          ),
          config=cel.NewEnvConfigFromYaml("""
            container:
              name: "test.container"
              aliases:
                - alias: "abc"
                  qualified_name: "x.y.baz"
          """),
      )
    self.assertIn(
        "Alias 'abc' is already defined with a different qualified name:"
        " x.y.baz",
        str(e.exception),
    )

  def test_config_export_variables(self):
    config: cel.Env = cel.NewEnv(
        variables={
            "var_bool": cel.Type.BOOL,
            "var_int": cel.Type.INT,
            "var_uint": cel.Type.UINT,
            "var_double": cel.Type.DOUBLE,
            "var_str": cel.Type.STRING,
            "var_bytes": cel.Type.BYTES,
            "var_msg": cel.Type("cel.expr.conformance.proto2.TestAllTypes"),
            "var_string_list": cel.Type.List(cel.Type.STRING),
            "var_timestamp": cel.Type.TIMESTAMP,
            "var_duration": cel.Type.DURATION,
            "var_dyn_list": cel.Type.LIST,
            "var_int_map": cel.Type.Map(cel.Type.INT, cel.Type.STRING),
            "var_string_map": cel.Type.Map(cel.Type.STRING, cel.Type.BOOL),
            "var_dyn_map": cel.Type.MAP,
            "var_dyn": cel.Type.DYN,
        }
    )
    yaml: str = config.config().to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
          variables:
            - name: "var_bool"
              type: "bool"
            - name: "var_bytes"
              type: "bytes"
            - name: "var_double"
              type: "double"
            - name: "var_duration"
              type: "duration"
            - name: "var_dyn"
              type: "dyn"
            - name: "var_dyn_list"
              type: "list<dyn>"
            - name: "var_dyn_map"
              type: "map<dyn,dyn>"
            - name: "var_int"
              type: "int"
            - name: "var_int_map"
              type: "map<int,string>"
            - name: "var_msg"
              type: "cel.expr.conformance.proto2.TestAllTypes"
            - name: "var_str"
              type: "string"
            - name: "var_string_list"
              type: "list<string>"
            - name: "var_string_map"
              type: "map<string,bool>"
            - name: "var_timestamp"
              type: "timestamp"
            - name: "var_uint"
              type: "uint"
      """),
    )

  def test_config_augmented_variables(self):
    config = cel.NewEnvConfigFromYaml("""
      variables:
        - name: "var_bool"
          type: "bool"
      """)
    env: cel.Env = cel.NewEnv(
        config=config,
        variables={
            "var_msg": cel.Type("cel.expr.conformance.proto2.TestAllTypes"),
        },
    )
    yaml: str = env.config().to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
          variables:
            - name: "var_bool"
              type: "bool"
            - name: "var_msg"
              type: "cel.expr.conformance.proto2.TestAllTypes"
        """),
    )

  def test_config_variable_override(self):
    config: cel.EnvConfig = cel.NewEnvConfigFromYaml("""
      variables:
        - name: "var_bool"
          type: "bool"
      """)

    with self.assertRaises(Exception) as e:
      cel.NewEnv(
          config=config,
          variables={
              "var_bool": cel.Type.INT,
          },
      )
    self.assertIn(
        "Variable 'var_bool' is already included",
        str(e.exception),
    )

  def test_config_variable_types(self):
    config: cel.EnvConfig = cel.NewEnvConfigFromYaml("""
      variables:
        - name: "var_bool"
          type: "bool"
        - name: "var_int"
          type: "int"
          value: 42
      """)
    env: cel.Env = cel.NewEnv(
        config=config,
        variables={
            "var_msg": cel.Type("cel.expr.conformance.proto2.TestAllTypes"),
        },
    )
    data: dict[str, Any] = {
        "var_bool": True,
        "var_msg": test_all_types_pb.TestAllTypes(single_string="hello"),
    }
    res: cel.Value = env.compile("var_bool").eval(data=data)
    self.assertEqual(res.type(), cel.Type.BOOL)
    self.assertTrue(res.value())

    res = env.compile("var_msg.single_string").eval(data=data)
    self.assertEqual(res.type(), cel.Type.STRING)
    self.assertEqual(res.value(), "hello")

    res = env.compile("var_int").eval(data=data)
    self.assertEqual(res.type(), cel.Type.INT)
    self.assertEqual(res.value(), 42)

  def test_config_export_extension_version(self):
    env: cel.Env = cel.NewEnv(
        extensions=[
            ext_math.ExtMath(0),
            ext_optional.ExtOptional(1),
            ext_strings.ExtStrings(2),
            ext_bindings.ExtBindings(),
        ],
    )
    yaml: str = env.config().to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
          extensions:
            - name: "bindings"
            - name: "math"
              version: 0
            - name: "optional"
              version: 1
            - name: "strings"
              version: 2
        """),
    )

  def test_config_extension_version_out_of_range(self):
    cases = [
        [
            lambda: ext_math.ExtMath(42),
            r"'math' extension version: 42 not in range \[0, \d+\]",
        ],
        [
            lambda: ext_optional.ExtOptional(6),
            r"'optional' extension version: 6 not in range \[0, \d+\]",
        ],
        [
            lambda: ext_strings.ExtStrings(18),
            r"'strings' extension version: 18 not in range \[0, \d+\]",
        ],
    ]
    for test_case in cases:
      with self.assertRaises(Exception) as e:
        cel.NewEnv(
            extensions=[test_case[0]()],
        )
      self.assertRegex(str(e.exception), test_case[1])

  def test_config_extensions(self):
    config: cel.EnvConfig = cel.NewEnvConfigFromYaml("""
      extensions:
        - name: math
        - name: strings
      """)
    env: cel.Env = cel.NewEnv(
        config=config,
        extensions=[TestCelExtension()],
    )
    yaml: str = env.config().to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
          extensions:
            - name: "math"
            - name: "strings"
            - name: "test_cel_extension"
        """),
    )
    res: cel.Value = env.compile("'%.4f'.format([math.sqrt(2)])").eval()
    self.assertEqual(res.value(), "1.4142")
    res = env.compile("hello('World')").eval()
    self.assertEqual(res.value(), "Hello, World!")

  def test_config_extension_override_same_version(self):
    config: cel.EnvConfig = cel.NewEnvConfigFromYaml("""
      extensions:
        - name: cel.lib.ext.math
          version: 1
        - name: strings
          version: 2
      """)
    env: cel.Env = cel.NewEnv(
        config=config,
        extensions=[ext_math.ExtMath(1), ext_strings.ExtStrings(2)],
    )
    res = env.compile("'%.3f'.format([math.floor(3.14)])").eval()
    self.assertEqual(res.value(), "3.000")

  def test_config_extension_override_different_version(self):
    config = cel.NewEnvConfigFromYaml("""
      extensions:
        - name: math
          version: 0
        - name: cel.lib.ext.strings
          version: 2
      """)
    with self.assertRaises(Exception) as e:
      cel.NewEnv(
          config=config,
          extensions=[ext_math.ExtMath()],
      )
    self.assertIn(
        "Extension 'math' version 0 is already included. Cannot"
        " also include version 2",
        str(e.exception),
    )
    with self.assertRaises(Exception) as e:
      cel.NewEnv(
          config=config,
          extensions=[ext_strings.ExtStrings(1)],
      )
    self.assertIn(
        "Extension 'cel.lib.ext.strings' version 2 is already included. Cannot"
        " also include version 1",
        str(e.exception),
    )

  def test_config_functions(self):
    config: cel.EnvConfig = cel.NewEnvConfigFromYaml("""
      functions:
        - name: is_ok
          overloads:
            - signature: "string.is_ok()"
              return: "bool"
      """)
    env: cel.Env = cel.NewEnv(
        config=config,
        functions=[
            cel.FunctionDecl(
                "hello",
                [
                    cel.Overload(
                        signature="hello(string,string)",
                        return_type=cel.Type.STRING,
                        impl=lambda ampm, arg: (
                            "Good"
                            f" {'morning' if ampm == 'am' else 'afternoon'},"
                            f" {arg}!"
                        ),
                    )
                ],
            )
        ],
        function_impls={
            "string.is_ok()": lambda arg: arg in ["excellent", "good", "fair"],
        },
    )
    yaml = env.config().to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
          functions:
            - name: "hello"
              overloads:
                - signature: "hello(string,string)"
                  return: "string"
            - name: "is_ok"
              overloads:
                - signature: "string.is_ok()"
                  return: "bool"
        """),
    )
    res: cel.Value = env.compile("hello('am', 'Sunshine')").eval()
    self.assertEqual(res.value(), "Good morning, Sunshine!")
    res = env.compile("hello('pm', 'tea is served')").eval()
    self.assertEqual(res.value(), "Good afternoon, tea is served!")
    res = env.compile("'good'.is_ok()").eval()
    self.assertTrue(res.value())
    res = env.compile("'bad'.is_ok()").eval()
    self.assertFalse(res.value())

  def test_config_function_override(self):
    config: cel.EnvConfig = cel.NewEnvConfigFromYaml("""
      functions:
        - name: foo
          overloads:
            - signature: "foo()"
      """)
    with self.assertRaises(Exception) as e:
      cel.NewEnv(
          config=config,
          functions=[
              cel.FunctionDecl(
                  "foo",
                  [
                      cel.Overload(
                          signature="foo()",
                          impl=lambda: "hello",
                      )
                  ],
              )
          ],
          function_impls={
              "foo()": lambda: "goodbye",
          },
      )
    self.assertIn(
        "An implementation for function overload 'foo()' already exists.",
        str(e.exception),
    )

  def test_overload_signature_errors(self):
    with self.assertRaises(ValueError) as e2:
      cel.Overload(signature="greet(string)", parameters=[cel.Type.STRING])
    self.assertIn(
        "If 'signature' is specified, 'parameters' should not be specified",
        str(e2.exception),
    )

    with self.assertRaises(ValueError) as e3:
      cel.Overload()
    self.assertIn(
        "Either 'id' or 'signature' must be specified", str(e3.exception)
    )

  def test_config_functions_deprecated_syntax(self):
    """Test that the deprecated function syntax is still supported."""
    config: cel.EnvConfig = cel.NewEnvConfigFromYaml("""
      functions:
        - name: is_ok
          overloads:
            - id: "is_ok_string"
              target:
                type_name: string
              return:
                type_name: bool
      """)
    env: cel.Env = cel.NewEnv(
        config=config,
        functions=[
            cel.FunctionDecl(
                "hello",
                [
                    cel.Overload(
                        "good_time_of_day",
                        return_type=cel.Type.STRING,
                        parameters=[
                            cel.Type.STRING,
                            cel.Type.STRING,
                        ],
                        impl=lambda ampm, arg: (
                            "Good"
                            f" {'morning' if ampm == 'am' else 'afternoon'},"
                            f" {arg}!"
                        ),
                    )
                ],
            )
        ],
        function_impls={
            "is_ok_string": lambda arg: arg in ["excellent", "good", "fair"],
        },
    )
    yaml = env.config().to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
          functions:
            - name: "hello"
              overloads:
                - id: "good_time_of_day"
                  signature: "hello(string,string)"
                  return: "string"
            - name: "is_ok"
              overloads:
                - id: "is_ok_string"
                  signature: "string.is_ok()"
                  return: "bool"
        """),
    )
    res: cel.Value = env.compile("hello('am', 'Sunshine')").eval()
    self.assertEqual(res.value(), "Good morning, Sunshine!")
    res = env.compile("hello('pm', 'tea is served')").eval()
    self.assertEqual(res.value(), "Good afternoon, tea is served!")
    res = env.compile("'good'.is_ok()").eval()
    self.assertTrue(res.value())
    res = env.compile("'bad'.is_ok()").eval()
    self.assertFalse(res.value())


class TestCelExtension(cel.CelExtension):
  """An example CEL extension for testing."""

  def __init__(self):
    super().__init__(
        "test_cel_extension",
        functions=[
            cel.FunctionDecl(
                "hello",
                [
                    cel.Overload(
                        "hello(string)",
                        return_type=cel.Type.STRING,
                        parameters=[
                            cel.Type.STRING,
                        ],
                        impl=lambda arg: f"Hello, {arg}!",
                    )
                ],
            ),
        ],
    )


def normalize_yaml(yaml: str) -> str:
  return textwrap.dedent(yaml).strip()


if __name__ == "__main__":
  absltest.main()
