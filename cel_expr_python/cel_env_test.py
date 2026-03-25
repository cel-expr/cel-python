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
        "| invalid yaml\n"
        "| ^",
        str(e.exception),
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
