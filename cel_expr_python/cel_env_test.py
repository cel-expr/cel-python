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

from absl.testing import absltest
from cel_expr_python import cel
from cel_expr_python.ext import ext_math
from cel.expr.conformance.proto2 import test_all_types_pb2 as test_all_types_pb


class CelEnvTest(absltest.TestCase):

  def test_env_config_from_and_to_yaml(self):
    config = cel.NewEnvConfigFromYaml("""
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
        type_name: int
        value: 1
      functions:
        - name: add
          overloads:
            - id: "add_int_int"
              args:
              - type_name: int
              - type_name: int
              return:
                type_name: int
    """)
    yaml = config.to_yaml()
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
          type_name: "int"
          value: 1
      functions:
        - name: "add"
          overloads:
            - id: "add_int_int"
              args:
                - type_name: "int"
                - type_name: "int"
              return:
                type_name: "int"
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

  def test_config_export_variables(self):
    config = cel.NewEnv(
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
    yaml = config.config().to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
          variables:
            - name: "var_bool"
              type_name: "bool"
            - name: "var_bytes"
              type_name: "bytes"
            - name: "var_double"
              type_name: "double"
            - name: "var_duration"
              type_name: "duration"
            - name: "var_dyn"
              type_name: "dyn"
            - name: "var_dyn_list"
              type_name: "list"
              params:
                - type_name: "dyn"
            - name: "var_dyn_map"
              type_name: "map"
              params:
                - type_name: "dyn"
                - type_name: "dyn"
            - name: "var_int"
              type_name: "int"
            - name: "var_int_map"
              type_name: "map"
              params:
                - type_name: "int"
                - type_name: "string"
            - name: "var_msg"
              type_name: "cel.expr.conformance.proto2.TestAllTypes"
            - name: "var_str"
              type_name: "string"
            - name: "var_string_list"
              type_name: "list"
              params:
                - type_name: "string"
            - name: "var_string_map"
              type_name: "map"
              params:
                - type_name: "string"
                - type_name: "bool"
            - name: "var_timestamp"
              type_name: "timestamp"
            - name: "var_uint"
              type_name: "uint"
      """),
    )

  def test_config_augmented_variables(self):
    config = cel.NewEnvConfigFromYaml("""
      variables:
        - name: "var_bool"
          type_name: "bool"
      """)
    env = cel.NewEnv(
        config=config,
        variables={
            "var_msg": cel.Type("cel.expr.conformance.proto2.TestAllTypes"),
        },
    )
    yaml = env.config().to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
          variables:
            - name: "var_bool"
              type_name: "bool"
            - name: "var_msg"
              type_name: "cel.expr.conformance.proto2.TestAllTypes"
        """),
    )

  def test_config_variable_override(self):
    config = cel.NewEnvConfigFromYaml("""
      variables:
        - name: "var_bool"
          type_name: "bool"
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
    config = cel.NewEnvConfigFromYaml("""
      variables:
        - name: "var_bool"
          type_name: "bool"
        - name: "var_int"
          type_name: "int"
          value: 42
      """)
    env = cel.NewEnv(
        config=config,
        variables={
            "var_msg": cel.Type("cel.expr.conformance.proto2.TestAllTypes"),
        },
    )
    data = {
        "var_bool": True,
        "var_msg": test_all_types_pb.TestAllTypes(single_string="hello"),
    }
    res = env.compile("var_bool").eval(data=data)
    self.assertEqual(res.type(), cel.Type.BOOL)
    self.assertTrue(res.value())

    res = env.compile("var_msg.single_string").eval(data=data)
    self.assertEqual(res.type(), cel.Type.STRING)
    self.assertEqual(res.value(), "hello")

    res = env.compile("var_int").eval(data=data)
    self.assertEqual(res.type(), cel.Type.INT)
    self.assertEqual(res.value(), 42)

  def test_config_extensions(self):
    config = cel.NewEnvConfigFromYaml("""
      extensions:
        - name: math
        - name: strings
      """)
    env = cel.NewEnv(
        config=config,
        extensions=[TestCelExtension()],
    )
    yaml = env.config().to_yaml()
    self.assertEqual(
        normalize_yaml(yaml),
        normalize_yaml("""
          extensions:
            - name: "math"
            - name: "strings"
            - name: "test_cel_extension"
        """),
    )
    res = env.compile("'%.4f'.format([math.sqrt(2)])").eval()
    self.assertEqual(res.value(), "1.4142")
    res = env.compile("hello('World')").eval()
    self.assertEqual(res.value(), "Hello, World!")

  def test_config_extensions_override(self):
    # TODO(b/498655870): add assertion based on extension aliases once
    # supported.
    config = cel.NewEnvConfigFromYaml("""
      extensions:
        - name: cel.lib.ext.math
          version: 0
        - name: cel.lib.ext.strings
      """)
    with self.assertRaises(Exception) as e:
      cel.NewEnv(
          config=config,
          extensions=[ext_math.ExtMath()],
      )
    self.assertIn(
        "Extension 'cel.lib.ext.math' version 0 is already included. Cannot"
        " also include version 'latest'",
        str(e.exception),
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
  lines = yaml.split("\n")
  indent = -1
  unindented_lines = []
  for line in lines:
    pos = -1
    for i, char in enumerate(line):
      if char != " " and char != "\t":
        pos = i
        break
    if pos == -1:
      # Skip blank lines.
      continue
    if indent == -1:
      indent = pos
    if pos >= indent:
      unindented_lines.append(line[indent:])
    else:
      unindented_lines.append(line)
  return "\n".join(unindented_lines)


if __name__ == "__main__":
  absltest.main()
