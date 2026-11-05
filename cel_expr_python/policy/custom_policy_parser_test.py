# Copyright 2026 Google LLC
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

from absl.testing import absltest
from python.runfiles import runfiles
from cel_expr_python.policy import cel_policy

_TESTDATA_FILE_PATH = "_main/cel_expr_python/policy/testdata"


class TestCustomYamlPolicyParser(cel_policy.CelPolicyYamlParser):
  """TestCustomYamlPolicyParser is used in unit tests for custom policy formats.

  It demonstrates the versatility of the cel::YamlPolicyParser framework API in
  Python by implementing custom tag and block parsing without needing to modify
  the core parser.
  """

  def __init__(self):
    super().__init__()
    self.log = []

  def parse_rule(
      self,
      ctx: cel_policy.CelPolicyParseContext,
      node: cel_policy.YAMLNode,
      rule: cel_policy.Rule,
  ) -> None:
    self.log.append("parse_rule")
    return super().parse_rule(ctx, node, rule)

  def parse_rule_tag(
      self,
      ctx: cel_policy.CelPolicyParseContext,
      tag_name: cel_policy.ValueString,
      node: cel_policy.YAMLNode,
      rule: cel_policy.Rule,
  ) -> bool:
    self.log.append(f"parse_rule_tag: {tag_name.value}")
    return super().parse_rule_tag(ctx, tag_name, node, rule)

  def parse_policy_tag(
      self,
      ctx: cel_policy.CelPolicyParseContext,
      tag_name: cel_policy.ValueString,
      node: cel_policy.YAMLNode,
  ) -> bool:
    self.log.append(f"parse_policy_tag: {tag_name.value}")
    if tag_name.value in ("name", "description", "imports"):
      return super().parse_policy_tag(ctx, tag_name, node)

    if tag_name.value == "inputs":
      if not node.is_sequence():
        ctx.report_error(tag_name.id, "Policy 'inputs' is not a sequence")
        return True

      rule: cel_policy.Rule = ctx.policy.rule
      vars_list: list[cel_policy.Variable] = []
      for _, var_node in enumerate(node):  # pyrefly: ignore[bad-argument-type]
        var: cel_policy.Variable = self.parse_variable(ctx, var_node, rule)
        vars_list.append(var)
      rule.variables = vars_list

      return True

    if tag_name.value == "purpose":
      purpose: cel_policy.ValueString | None = self.get_value_string(
          ctx, node, "Policy purpose is not a string"
      )
      if purpose is not None:
        ctx.policy.metadata["purpose"] = purpose.value
      return True

    if tag_name.value == "version":
      version: cel_policy.ValueString | None = self.get_value_string(
          ctx, node, "Policy version is not a string"
      )
      if version is None:
        return True
      try:
        version_int: int = int(version.value)
      except ValueError:
        ctx.report_error(
            version.id, f"Policy version is not an integer: {version.value}"
        )
        return True
      ctx.policy.metadata["version"] = version_int
      return True

    if tag_name.value == "conditions":
      if not node.is_sequence():
        ctx.report_error(tag_name.id, "Policy 'conditions' is not a sequence")
        return True

      rule: cel_policy.Rule = ctx.policy.rule
      for _, condition in enumerate(node):  # pyrefly: ignore[bad-argument-type]
        # Track the number of existing matches before parsing. When ParseMatch
        # evaluates an 'else' block, it recursively triggers parsing and adds
        # internal inner matches directly to the rule's match vector.
        # Inserting the outer match at begin() + size_before ensures that the
        # primary outer 'if' condition is always evaluated before its nested
        # 'else' fallbacks.
        size_before = len(rule.matches)
        match = self.parse_match(ctx, condition, rule)
        rule.matches.insert(size_before, match)

      return True

    return False

  def parse_match(
      self,
      ctx: cel_policy.CelPolicyParseContext,
      node: cel_policy.YAMLNode,
      rule: cel_policy.Rule,
  ) -> cel_policy.Match:
    self.log.append("parse_match")
    return super().parse_match(ctx, node, rule)

  def parse_match_tag(
      self,
      ctx: cel_policy.CelPolicyParseContext,
      tag_name: cel_policy.ValueString,
      node: cel_policy.YAMLNode,
      match: cel_policy.Match,
      rule: cel_policy.Rule,
  ) -> bool:
    self.log.append(f"parse_match_tag: {tag_name.value}")
    if tag_name.value == "if":
      condition: cel_policy.ValueString | None = self.get_value_string(
          ctx, node, "Policy 'if' condition is not a string"
      )
      if condition is not None:
        match.condition = condition  # pyrefly: ignore[read-only]
      return True
    if tag_name.value == "then":
      self.parse_then_block(ctx, node, match)
      return True
    if tag_name.value == "else":
      self.parse_else_block(ctx, node, rule)
      return True
    return False

  def parse_then_block(
      self,
      ctx: cel_policy.CelPolicyParseContext,
      value_node: cel_policy.YAMLNode,
      match: cel_policy.Match,
  ):
    if value_node.is_scalar():
      val: cel_policy.ValueString | None = self.get_value_string(
          ctx, value_node, "Policy condition 'then' is not a string"
      )
      if val is not None:
        output: cel_policy.OutputBlock = cel_policy.OutputBlock()
        output.output = val
        match.set_result(output)
    elif value_node.is_map():
      nested_rule: cel_policy.Rule = cel_policy.Rule()
      nested_match: cel_policy.Match = self.parse_match(
          ctx, value_node, nested_rule
      )
      nested_rule.matches.insert(0, nested_match)
      match.set_result(nested_rule)
    else:
      id_val: int = ctx.collect_metadata(value_node)
      ctx.report_error(id_val, "Bad syntax in 'if/then' block")

  def parse_else_block(
      self,
      ctx: cel_policy.CelPolicyParseContext,
      value_node: cel_policy.YAMLNode,
      rule: cel_policy.Rule,
  ):
    if value_node.is_scalar():
      val: cel_policy.ValueString | None = self.get_value_string(
          ctx, value_node, "Policy condition 'else' is not a string"
      )
      if val is not None:
        else_match: cel_policy.Match = cel_policy.Match()
        else_match.id = ctx.collect_metadata(value_node)  # pyrefly: ignore[read-only]
        output: cel_policy.OutputBlock = cel_policy.OutputBlock()
        output.output = val
        else_match.set_result(output)
        rule.matches.append(else_match)
    elif value_node.is_map():
      size_before: int = len(rule.matches)
      match: cel_policy.Match = self.parse_match(ctx, value_node, rule)
      rule.matches.insert(size_before, match)
    else:
      id_val: int = ctx.collect_metadata(value_node)
      ctx.report_error(id_val, "Bad syntax in 'if/then' block")

  def parse_variable(
      self,
      ctx: cel_policy.CelPolicyParseContext,
      node: cel_policy.YAMLNode,
      rule: cel_policy.Rule,
  ) -> cel_policy.Variable:
    self.log.append("parse_variable")
    return super().parse_variable(ctx, node, rule)

  def parse_variable_tag(
      self,
      ctx: cel_policy.CelPolicyParseContext,
      tag_name: cel_policy.ValueString,
      node: cel_policy.YAMLNode,
      variable: cel_policy.Variable,
  ) -> bool:
    self.log.append(f"parse_variable_tag: {tag_name.value}")
    if tag_name.value == "name":
      return super().parse_variable_tag(ctx, tag_name, node, variable)
    if tag_name.value == "assign":
      expression: cel_policy.ValueString | None = self.get_value_string(
          ctx, node, "Variable 'assign' expression is not a string"
      )
      if expression is not None:
        variable.expression = expression  # pyrefly: ignore[read-only]
      return True
    return False


class CustomPolicyParserTest(absltest.TestCase):

  def test_custom_policy_format(self):
    yaml_content: str = load_policy_yaml("custom_policy_format.yaml")

    parser: TestCustomYamlPolicyParser = TestCustomYamlPolicyParser()
    result: cel_policy.CelPolicyParseResult = cel_policy.parse_yaml_cel_policy(
        yaml_content, parser
    )

    self.assertTrue(result.is_valid, result.formatted_issues())

    self.assertEqual(
        parser.log,
        [
            "parse_policy_tag: name",
            "parse_policy_tag: description",
            "parse_policy_tag: imports",
            "parse_policy_tag: purpose",
            "parse_policy_tag: version",
            "parse_policy_tag: inputs",
            "parse_variable",
            "parse_variable_tag: name",
            "parse_variable_tag: assign",
            "parse_policy_tag: conditions",
            "parse_match",
            "parse_match_tag: if",
            "parse_match_tag: then",
            "parse_match_tag: else",
            "parse_match",
            "parse_match_tag: if",
            "parse_match_tag: then",
            "parse_match",
            "parse_match_tag: if",
            "parse_match_tag: then",
            "parse_match_tag: else",
            "parse_match_tag: else",
        ],
    )

    policy = result.policy()
    self.assertEqual(policy.name.value, "cel_policy_custom_tags")
    description = policy.description
    self.assertIsNotNone(description)
    self.assertEqual(description.value, "A custom policy format")
    self.assertEqual(policy.metadata["purpose"], "test")
    self.assertEqual(policy.metadata["version"], 42)
    self.assertLen(policy.imports, 1)
    self.assertEqual(
        policy.imports[0].name.value,
        "cel.expr.conformance.proto3.TestAllTypes",
    )
    self.assertLen(policy.rule.variables, 1)
    self.assertEqual(policy.rule.variables[0].name.value, "test_input")
    self.assertEqual(policy.rule.variables[0].expression.value, "42")

  def test_custom_policy_format_with_errors(self):
    yaml_content: str = load_policy_yaml(
        "custom_policy_format_with_errors.yaml"
    )

    result: cel_policy.CelPolicyParseResult = cel_policy.parse_yaml_cel_policy(
        yaml_content,
        TestCustomYamlPolicyParser(),
        description="custom_policy_format_with_errors.yaml",
    )

    self.assertFalse(result.is_valid)
    formatted_issues: str = result.formatted_issues()
    self.assertIn(
        "custom_policy_format_with_errors.yaml:"
        "19:3: Policy purpose is not a string",
        formatted_issues,
    )
    self.assertIn(
        "custom_policy_format_with_errors.yaml:"
        "20:10: Policy version is not an integer: new",
        formatted_issues,
    )
    self.assertIn(
        "custom_policy_format_with_errors.yaml:"
        "23:5: Policy 'if' condition is not a string",
        formatted_issues,
    )
    self.assertIn(
        "custom_policy_format_with_errors.yaml:"
        "31:7: Bad syntax in 'if/then' block",
        formatted_issues,
    )


def load_policy_yaml(filename: str) -> str:
  r = runfiles.Create()
  path = r.Rlocation(f"{_TESTDATA_FILE_PATH}/{filename}")
  with open(path, "rt") as f:
    return f.read()


if __name__ == "__main__":
  absltest.main()
