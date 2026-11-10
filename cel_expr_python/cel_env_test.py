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

import datetime
import textwrap
from typing import Any

from absl.testing import absltest
from absl.testing import parameterized
from cel.expr.conformance.proto2 import test_all_types_pb2 as test_all_types_pb
from cel_expr_python import cel
from cel_expr_python.ext import ext_bindings
from cel_expr_python.ext import ext_math
from cel_expr_python.ext import ext_optional
from cel_expr_python.ext import ext_strings


class CelEnvTest(parameterized.TestCase):

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
            "var_opt_int": cel.Type.Optional(cel.Type.INT),
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
            - name: "var_opt_int"
              type: "optional_type<int>"
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
            extensions=[test_case[0]()],  # pyrefly: ignore[not-callable]
        )
      self.assertRegex(str(e.exception), test_case[1])  # pyrefly: ignore[bad-argument-type]

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

  def test_standard_extensions_via_config(self):
    config: cel.EnvConfig = cel.NewEnvConfigFromYaml("""
      extensions:
        - name: lists
        - name: sets
        - name: two-var-comprehensions
        - name: optional
        - name: regex
      """)
    env: cel.Env = cel.NewEnv(config=config)

    # Test lists extension (e.g., range and reverse)
    expr = env.compile("lists.range(3)")
    self.assertEqual(expr.eval(), [0, 1, 2])
    self.assertEqual(expr.eval().value(), [0, 1, 2])
    res = env.compile("[1, 2].reverse()").eval()
    self.assertEqual(res, [2, 1])
    self.assertEqual(res.value(), [2, 1])
    self.assertEqual(res.plain_value(), [2, 1])

    # Test sets extension (e.g., intersects)
    res = env.compile("sets.intersects([1, 2], [2, 3])").eval()
    self.assertEqual(res.value(), True)

    # Test two-var-comprehensions (e.g., all)
    res = env.compile("[1, 2].all(i, v, v > 0)").eval()
    self.assertEqual(res.value(), True)

    # Test regex extension (e.g., replace)
    expr = r"regex.replace('123-456', r'(\d+)-(\d+)', r'\2-\1')"
    res = env.compile(expr).eval()
    self.assertEqual(res.value(), "456-123")

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

  def test_env_options(self):
    options = cel.Options()
    self.assertTrue(options.enable_pratt_parser)
    self.assertEqual(repr(options), "Options(enable_pratt_parser=True)")
    options.enable_pratt_parser = False
    self.assertFalse(options.enable_pratt_parser)
    self.assertEqual(repr(options), "Options(enable_pratt_parser=False)")

    env = cel.NewEnv(options=cel.Options(enable_pratt_parser=False))
    self.assertFalse(env.options().enable_pratt_parser)

    default_env = cel.NewEnv()
    self.assertTrue(default_env.options().enable_pratt_parser)

  def _new_poly_env(self) -> cel.Env:
    ext = cel.CelExtension(
        "poly_ext",
        functions=[
            cel.FunctionDecl(
                "ext_id",
                [
                    cel.Overload(
                        signature="ext_id(~T)",
                        return_type=cel.Type("~T"),
                        impl=lambda x: x,
                    )
                ],
            )
        ],
    )
    return cel.NewEnv(
        extensions=[ext, ext_optional.ExtOptional()],
        variables={
            "var_tp": cel.Type("~T"),
            "var_type": cel.Type.TYPE,
            "var_int_type": cel.Type.Type(cel.Type.INT),
            "var_dyn": cel.Type.DYN,
        },
        functions=[
            cel.FunctionDecl(
                "to_string",
                [
                    cel.Overload(
                        signature="to_string(dyn)",
                        return_type=cel.Type.STRING,
                        impl=repr,
                    )
                ],
            ),
            cel.FunctionDecl(
                "dyn_id",
                [
                    cel.Overload(
                        signature="dyn_id(dyn)",
                        return_type=cel.Type.DYN,
                        impl=lambda x: x,
                    )
                ],
            ),
            cel.FunctionDecl(
                "identity",
                [
                    cel.Overload(
                        signature="identity(~T)",
                        return_type=cel.Type("~T"),
                        impl=lambda x: x,
                    )
                ],
            ),
            cel.FunctionDecl(
                "tp_id",
                [
                    cel.Overload(
                        signature="tp_id(~T)",
                        return_type=cel.Type("~T"),
                        impl=lambda x: x,
                    )
                ],
            ),
            cel.FunctionDecl(
                "bad_type_ret",
                [
                    cel.Overload(
                        signature="bad_type_ret()",
                        return_type=cel.Type.TYPE,
                        impl=lambda: "not_a_type",
                    )
                ],
            ),
            cel.FunctionDecl(
                "make_tp",
                [
                    cel.Overload(
                        signature="make_tp()",
                        return_type=cel.Type("~T"),
                    )
                ],
            ),
            cel.FunctionDecl(
                "late_ovl",
                [
                    cel.Overload(
                        signature="late_ovl(int)",
                        return_type=cel.Type.STRING,
                    ),
                    cel.Overload(
                        signature="late_ovl(string)",
                        return_type=cel.Type.STRING,
                    ),
                ],
            ),
            cel.FunctionDecl(
                "late_dyn",
                [
                    cel.Overload(
                        signature="late_dyn(dyn)",
                        return_type=cel.Type.STRING,
                    ),
                ],
            ),
            cel.FunctionDecl(
                "late_tp",
                [
                    cel.Overload(
                        signature="late_tp(~T)",
                        return_type=cel.Type("~T"),
                    ),
                ],
            ),
        ],
    )

  def test_functions_with_dyn_and_type_params(self):
    env = self._new_poly_env()
    self.assertEqual(env.compile("to_string(42)").eval().value(), "42")
    self.assertEqual(env.compile("identity('abc')").eval().value(), "abc")
    self.assertEqual(env.compile("ext_id(7) + 1").eval().value(), 8)

    # Type-parameterized variables, parameterized/abstract TypeValues, invalid
    # TYPE conversions, and overloaded activation functions.
    self.assertEqual(env.compile("var_tp").return_type(), cel.Type.DYN)
    self.assertEqual(env.compile("make_tp()").return_type(), cel.Type.DYN)
    self.assertEqual(
        env.compile("tp_id(var_type)")
        .eval(data={"var_type": cel.Type.Type(cel.Type.INT)})
        .value(),
        cel.Type.Type(cel.Type.INT),
    )
    self.assertEqual(
        env.compile("tp_id(var_int_type)")
        .eval(data={"var_int_type": cel.Type.INT})
        .value(),
        cel.Type.INT,
    )
    abstract_type = cel.Type.AbstractType("optional_type", [cel.Type.INT])
    self.assertEqual(
        env.compile("tp_id(var_dyn)")
        .eval(data={"var_dyn": abstract_type})
        .value(),
        abstract_type,
    )
    coll_type = cel.Type.AbstractType("collection", [cel.Type.DOUBLE])
    self.assertEqual(
        env.compile("tp_id(var_type)")
        .eval(data={"var_type": coll_type})
        .value(),
        coll_type,
    )
    self.assertEqual(
        env.compile("tp_id(var_type)")
        .eval(data={"var_type": cel.Type.Type(coll_type)})
        .value(),
        cel.Type.Type(coll_type),
    )
    bad_type_var_res = env.compile("var_type").eval(
        data={"var_type": "not_a_type"}
    )
    self.assertEqual(bad_type_var_res.type(), cel.Type.ERROR)
    self.assertIn(
        "Unexpected value type for 'var_type': str. (Expected TYPE<DYN>)",
        bad_type_var_res.value(),
    )
    bad_type_fn_res = env.compile("bad_type_ret()").eval()
    self.assertEqual(bad_type_fn_res.type(), cel.Type.ERROR)
    self.assertIn("(Expected TYPE<DYN>)", bad_type_fn_res.value())

    ovl_act = env.Activation(
        {},
        functions=[
            cel.Function(
                "late_ovl",
                [cel.Type.INT],
                False,
                lambda x: f"int:{x}",
                return_type=cel.Type.STRING,
            ),
            cel.Function(
                "late_ovl",
                [cel.Type.STRING],
                False,
                lambda x: f"str:{x}",
                return_type=cel.Type.STRING,
            ),
        ],
    )
    self.assertEqual(env.compile("late_ovl(1)").eval(ovl_act).value(), "int:1")
    self.assertEqual(
        env.compile("late_ovl('a')").eval(ovl_act).value(), "str:a"
    )

    poly_act = env.Activation(
        {},
        functions=[
            cel.Function(
                "late_dyn",
                [cel.Type.DYN],
                False,
                repr,
                return_type=cel.Type.STRING,
            ),
            cel.Function(
                "late_tp",
                [cel.Type("~T")],
                False,
                lambda x: x,
                return_type=cel.Type("~T"),
            ),
        ],
    )
    self.assertEqual(env.compile("late_dyn(42)").eval(poly_act).value(), "42")
    self.assertEqual(
        env.compile("late_dyn('abc')").eval(poly_act).value(), "'abc'"
    )
    self.assertEqual(env.compile("late_tp(7)").eval(poly_act).value(), 7)
    self.assertEqual(
        env.compile("late_tp('abc')").eval(poly_act).value(), "abc"
    )

  @parameterized.named_parameters(
      ("null_eq", "ext_id(null) == null", True),
      ("bool_not", "!ext_id(false)", True),
      ("int_add", "identity(7) + 1", 8),
      ("uint_add", "ext_id(9223372036854775808u) + 1u", 9223372036854775809),
      ("double_add", "identity(1.5) + 2.5", 4.0),
      ("string_add", "ext_id('hello') + '!'", "hello!"),
      ("bytes_add", "identity(b'ab') + b'cd'", bytearray(b"abcd")),
      ("list_add", "ext_id([1, 2]) + [3]", [1, 2, 3]),
      ("map_index", "identity({'a': 1})['a']", 1),
      ("type_eq", "ext_id(type(1)) == int", True),
      ("type_list_eq", "ext_id(type([1])) == list", True),
      ("type_map_eq", "ext_id(type({'a': 1})) == map", True),
      (
          "type_ts_eq",
          (
              "ext_id(type(timestamp('2025-01-01T00:00:00Z'))) =="
              " google.protobuf.Timestamp"
          ),
          True,
      ),
      (
          "type_dur_eq",
          "ext_id(type(duration('1s'))) == google.protobuf.Duration",
          True,
      ),
      (
          "timestamp_add",
          "identity(timestamp('2025-01-01T00:00:00Z')) + duration('1h')",
          datetime.datetime(2025, 1, 1, 1, 0, tzinfo=datetime.timezone.utc),
      ),
      (
          "duration_add",
          "ext_id(duration('10s')) + duration('5s')",
          datetime.timedelta(seconds=15),
      ),
      (
          "proto_field",
          (
              "ext_id(cel.expr.conformance.proto2.TestAllTypes"
              "{single_string: 'hello'}).single_string"
          ),
          "hello",
      ),
      (
          "wrapper_add",
          "ext_id(google.protobuf.StringValue{value: 'wrap'}) + 'ped'",
          "wrapped",
      ),
  )
  def test_type_param_function_in_expressions(
      self,
      expr_str: str,
      expected_val: Any,
  ):
    env = self._new_poly_env()
    res = env.compile(expr_str).eval()
    self.assertEqual(res.value(), expected_val)

  @parameterized.named_parameters(
      ("null", "null", None, cel.Type.NULL),
      ("bool_true", "true", True, cel.Type.BOOL),
      ("bool_false", "false", False, cel.Type.BOOL),
      ("int", "-42", -42, cel.Type.INT),
      (
          "uint_large",
          "18446744073709551615u",
          18446744073709551615,
          cel.Type.UINT,
      ),
      ("double", "3.14", 3.14, cel.Type.DOUBLE),
      ("string", "'abc'", "abc", cel.Type.STRING),
      ("bytes", r"b'ab\x00c'", bytearray(b"ab\x00c"), cel.Type.BYTES),
      ("list", "[1, 'two', true]", [1, "two", True], cel.Type.LIST),
      ("map", "{'a': 1, 'b': 'two'}", {"a": 1, "b": "two"}, cel.Type.MAP),
      (
          "timestamp",
          "timestamp('2025-06-15T12:30:00.500000Z')",
          datetime.datetime(
              2025, 6, 15, 12, 30, 0, 500000, datetime.timezone.utc
          ),
          cel.Type.TIMESTAMP,
      ),
      (
          "duration",
          "duration('10s')",
          datetime.timedelta(seconds=10),
          cel.Type.DURATION,
      ),
      ("type_int", "type(1)", cel.Type.INT, cel.Type.TYPE),
      ("type_null", "type(null)", cel.Type.NULL, cel.Type.TYPE),
      ("type_type", "type(int)", cel.Type.TYPE, cel.Type.TYPE),
      (
          "type_msg",
          "type(cel.expr.conformance.proto2.TestAllTypes{})",
          cel.Type("cel.expr.conformance.proto2.TestAllTypes"),
          cel.Type.TYPE,
      ),
      (
          "proto_msg",
          (
              "cel.expr.conformance.proto2.TestAllTypes"
              "{single_string: 'hello', single_int32: 7}"
          ),
          test_all_types_pb.TestAllTypes(single_string="hello", single_int32=7),
          cel.Type("cel.expr.conformance.proto2.TestAllTypes"),
      ),
      (
          "wkt_bool_value",
          "google.protobuf.BoolValue{value: true}",
          True,
          cel.Type.BOOL,
      ),
      (
          "wkt_int32_value",
          "google.protobuf.Int32Value{value: -12}",
          -12,
          cel.Type.INT,
      ),
      (
          "wkt_int64_value",
          "google.protobuf.Int64Value{value: 42}",
          42,
          cel.Type.INT,
      ),
      (
          "wkt_uint64_value",
          "google.protobuf.UInt64Value{value: 18446744073709551615u}",
          18446744073709551615,
          cel.Type.UINT,
      ),
      (
          "wkt_float_value",
          "google.protobuf.FloatValue{value: 1.5}",
          1.5,
          cel.Type.DOUBLE,
      ),
      (
          "wkt_double_value",
          "google.protobuf.DoubleValue{value: 2.5}",
          2.5,
          cel.Type.DOUBLE,
      ),
      (
          "wkt_string_value",
          "google.protobuf.StringValue{value: 'wrapped'}",
          "wrapped",
          cel.Type.STRING,
      ),
      (
          "wkt_bytes_value",
          "google.protobuf.BytesValue{value: b'bin'}",
          bytearray(b"bin"),
          cel.Type.BYTES,
      ),
      (
          "wkt_empty_wrapper",
          "google.protobuf.Int64Value{}",
          0,
          cel.Type.INT,
      ),
      (
          "wkt_unset_wrapper_field",
          "cel.expr.conformance.proto2.TestAllTypes{}.single_int64_wrapper",
          None,
          cel.Type.NULL,
      ),
      (
          "wkt_empty_value",
          "google.protobuf.Value{}",
          None,
          cel.Type.NULL,
      ),
      (
          "wkt_struct",
          (
              "google.protobuf.Struct"
              "{fields: {'k': google.protobuf.Value{string_value: 'v'}}}"
          ),
          {"k": "v"},
          cel.Type.MAP,
      ),
      (
          "wkt_list_value",
          (
              "google.protobuf.ListValue"
              "{values: [google.protobuf.Value{number_value: 2.0}]}"
          ),
          [2.0],
          cel.Type.LIST,
      ),
      (
          "wkt_value",
          "google.protobuf.Value{bool_value: true}",
          True,
          cel.Type.BOOL,
      ),
      (
          "wkt_any",
          (
              "cel.expr.conformance.proto2.TestAllTypes"
              "{single_any: cel.expr.conformance.proto2.TestAllTypes"
              "{single_string: 'packed'}}.single_any"
          ),
          test_all_types_pb.TestAllTypes(single_string="packed"),
          cel.Type("cel.expr.conformance.proto2.TestAllTypes"),
      ),
      (
          "nested_list",
          "[timestamp('2025-06-15T12:30:00.500000Z'), duration('5s'), type(1)]",
          [
              datetime.datetime(
                  2025, 6, 15, 12, 30, 0, 500000, datetime.timezone.utc
              ),
              datetime.timedelta(seconds=5),
              cel.Type.INT,
          ],
          cel.Type.LIST,
      ),
      (
          "nested_map",
          "{1: duration('2s'), 2: type('a')}",
          {1: datetime.timedelta(seconds=2), 2: cel.Type.STRING},
          cel.Type.MAP,
      ),
  )
  def test_dyn_function_values(
      self,
      expr_str: str,
      expected_val: Any,
      expected_type: cel.Type,
  ):
    env = self._new_poly_env()
    res = env.compile(f"dyn_id({expr_str})").eval()
    self.assertEqual(res.type(), expected_type)
    self.assertEqual(res.value(), expected_val)
    self.assertEqual(res.plain_value(), expected_val)

  @parameterized.named_parameters(
      (
          "list",
          "type([1])",
          "list",
          cel.Type.Type(cel.Type.List(cel.Type.INT)),
          cel.Type.LIST,
      ),
      (
          "list_cross_elem",
          "type([1])",
          "type(['a'])",
          cel.Type.Type(cel.Type.List(cel.Type.INT)),
          cel.Type.LIST,
      ),
      (
          "map",
          "type({'a': 1})",
          "map",
          cel.Type.Type(cel.Type.Map(cel.Type.STRING, cel.Type.INT)),
          cel.Type.MAP,
      ),
      (
          "type_int",
          "type(int)",
          "type",
          cel.Type.Type(cel.Type.Type(cel.Type.INT)),
          cel.Type.TYPE,
      ),
      (
          "type_type_1",
          "type(type(1))",
          "type",
          cel.Type.Type(cel.Type.Type(cel.Type.INT)),
          cel.Type.TYPE,
      ),
      (
          "optional_of",
          "type(optional.of(1))",
          "optional_type",
          cel.Type.Type(cel.Type.AbstractType("optional_type", [cel.Type.INT])),
          cel.Type.AbstractType("optional_type", [cel.Type.DYN]),
      ),
      (
          "optional_of_cross_elem",
          "type(optional.of(1))",
          "type(optional.of('a'))",
          cel.Type.Type(cel.Type.AbstractType("optional_type", [cel.Type.INT])),
          cel.Type.AbstractType("optional_type", [cel.Type.DYN]),
      ),
      (
          "optional_none",
          "type(optional.none())",
          "optional_type",
          cel.Type.Type(cel.Type.AbstractType("optional_type", [cel.Type.DYN])),
          cel.Type.AbstractType("optional_type", [cel.Type.DYN]),
      ),
  )
  def test_runtime_type_erasure(
      self,
      type_expr: str,
      expected_cel_type: str,
      expected_static_type: cel.Type,
      expected_runtime_val: cel.Type,
  ):
    env = self._new_poly_env()
    expr = env.compile(type_expr)
    self.assertEqual(expr.return_type(), expected_static_type)
    self.assertEqual(expr.eval().value(), expected_runtime_val)
    self.assertTrue(
        env.compile(f"{type_expr} == {expected_cel_type}").eval().value()
    )
    tp_expr = env.compile(f"tp_id({type_expr})")
    self.assertEqual(tp_expr.return_type(), expected_static_type)
    self.assertEqual(tp_expr.eval().value(), expected_runtime_val)
    self.assertTrue(
        env.compile(f"tp_id({type_expr}) == {expected_cel_type}").eval().value()
    )

  def test_abstract_type_config_and_value_type(self):
    env = cel.NewEnv(
        variables={
            "var_coll": cel.Type.AbstractType("collection", [cel.Type.DOUBLE])
        },
        extensions=[ext_optional.ExtOptional(1)],
    )
    self.assertEqual(
        normalize_yaml(env.config().to_yaml()),
        normalize_yaml("""
          extensions:
            - name: "optional"
              version: 1
          variables:
            - name: "var_coll"
              type: "collection<double>"
        """),
    )
    res = env.compile("optional.none()").eval()
    self.assertEqual(
        res.type(), cel.Type.AbstractType("optional_type", [cel.Type.DYN])
    )
    coll_ext = cel.CelExtension(
        "coll_ext",
        functions=[
            cel.FunctionDecl(
                "coll_id",
                [
                    cel.Overload(
                        "coll_id_ovl",
                        cel.Type.AbstractType("collection", [cel.Type.DOUBLE]),
                        [
                            cel.Type.AbstractType(
                                "collection", [cel.Type.DOUBLE]
                            )
                        ],
                        impl=lambda x: x,
                    )
                ],
            )
        ],
    )
    coll_env = cel.NewEnv(
        variables={
            "var_coll": cel.Type.AbstractType("collection", [cel.Type.DOUBLE])
        },
        extensions=[coll_ext],
    )
    self.assertEqual(
        coll_env.compile("coll_id(var_coll)").return_type(),
        cel.Type.AbstractType("collection", [cel.Type.DOUBLE]),
    )

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
                        signature="hello(string)",
                        return_type=cel.Type.STRING,
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
