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

from __future__ import annotations

import gc

from absl.testing import absltest
from python.runfiles import runfiles
from cel.expr.conformance.proto3 import test_all_types_pb2
from cel_expr_python import cel
from cel_expr_python.ext import ext_bindings
from cel_expr_python.ext import ext_optional
from cel_expr_python.policy import cel_policy

_TESTDATA_FILE_PATH = "_main/cel_expr_python/policy/testdata"


class PolicyTest(absltest.TestCase):

  def test_parse_yaml_cel_policy(self):
    result = cel_policy.parse_yaml_cel_policy(
        load_policy_yaml("cel_policy.yaml")
    )
    self.assertTrue(result.is_valid)

    policy: cel_policy.CelPolicy = result.policy()
    self.assertEqual(policy.name.value, "cel_policy")

    description: cel_policy.ValueString | None = policy.description
    self.assertIsNotNone(description)
    self.assertEqual(description.value, "A test policy for CEL")

    display_name: cel_policy.ValueString | None = policy.display_name
    self.assertIsNotNone(display_name)
    self.assertEqual(display_name.value, "Cel Policy")

    self.assertLen(policy.imports, 2)
    self.assertEqual(
        policy.imports[0].name.value,
        "cel.expr.conformance.proto3.TestAllTypes",
    )
    self.assertEqual(
        policy.imports[1].name.value,
        "cel.expr.conformance.proto3.TestAllTypes.NestedEnum",
    )

    rule: cel_policy.Rule = policy.rule
    self.assertFalse(rule.is_aggregate)

    rule_id: cel_policy.ValueString | None = rule.rule_id
    self.assertIsNotNone(rule_id)
    self.assertEqual(rule_id.value, "test_rule")

    rule_description: cel_policy.ValueString | None = rule.description
    self.assertIsNotNone(rule_description)
    self.assertEqual(rule_description.value, "test rule description")

    self.assertLen(rule.variables, 1)
    var: cel_policy.Variable = rule.variables[0]
    self.assertEqual(var.name.value, "test_var")

    self.assertLen(rule.matches, 2)
    match0: cel_policy.Match = rule.matches[0]
    self.assertEqual(
        match0.condition.value.strip(),
        "spec.single_int32 > TestAllTypes{single_int64: 10}.single_int64",
    )
    self.assertTrue(match0.has_output_block)
    self.assertEqual(
        match0.output_block.output.value.strip(),
        '"invalid spec, got single_int32=" + string(spec.single_int32) + '
        '", wanted <= 10"',
    )

  def test_cel_policy_yaml_parser(self):
    result = cel_policy.parse_yaml_cel_policy(
        load_policy_yaml("cel_policy.yaml")
    )
    policy = result.policy()

    self.assertEmpty(policy.metadata)
    self.assertNotIn("key1", policy.metadata)

    policy.metadata["key1"] = "value1"
    self.assertEqual(policy.metadata["key1"], "value1")
    self.assertIn("key1", policy.metadata)
    self.assertLen(policy.metadata, 1)

    self.assertEqual(list(policy.metadata), ["key1"])
    self.assertEqual(list(policy.metadata.keys()), ["key1"])
    self.assertEqual(list(policy.metadata.values()), ["value1"])
    self.assertEqual(list(policy.metadata.items()), [("key1", "value1")])

    self.assertRegex(repr(policy), r"metadata:\s*\{\s*key1:\s*value1\s*}")

  def _compile_test_policy(
      self,
      filename: str,
      options: cel_policy.CompilePolicyOptions | None = None,
  ) -> cel_policy.CelPolicyValidationResult:
    result = cel_policy.parse_yaml_cel_policy(load_policy_yaml(filename))
    policy = result.policy()

    env = cel.NewEnv(
        variables={
            "spec": cel.Type("cel.expr.conformance.proto3.TestAllTypes")
        },
        extensions=[
            ext_bindings.ExtBindings(),
            ext_optional.ExtOptional(),
        ],
    )
    return cel_policy.compile(env, policy, options)

  def test_compile_policy_valid(self):
    result = self._compile_test_policy(
        "cel_policy.yaml", {"unnesting_height_limit": 10}
    )
    self.assertTrue(result.is_valid, result.formatted_issues())
    self.assertIsNotNone(result.expression())

  def test_compile_policy_invalid(self):
    result = self._compile_test_policy("cel_policy_invalid.yaml")
    self.assertFalse(result.is_valid)
    self.assertIn(
        "condition must evaluate to bool",
        result.formatted_issues(),
    )
    with self.assertRaises(RuntimeError):
      result.expression()

  def test_compile_policy_invalid_env_type(self):
    result = cel_policy.parse_yaml_cel_policy(
        load_policy_yaml("cel_policy.yaml")
    )
    policy = result.policy()
    with self.assertRaises(TypeError):
      cel_policy.compile(None, policy)  # pyrefly: ignore[bad-argument-type]
    with self.assertRaises(TypeError):
      cel_policy.compile("not_an_env", policy)  # pyrefly: ignore[bad-argument-type]

  def test_compile_policy_invalid_options_type(self):
    with self.assertRaises(RuntimeError):
      self._compile_test_policy(
          "cel_policy.yaml",
          {"unnesting_height_limit": "ten"},  # pyrefly: ignore[bad-argument-type]
      )

  def test_compile_policy_unnesting_height_limit_too_small(self):
    with self.assertRaisesRegex(
        RuntimeError, "unnesting_height_limit must be at least 2"
    ):
      self._compile_test_policy(
          "cel_policy.yaml", {"unnesting_height_limit": 1}
      )

  def test_compile_policy_default_options(self):
    result = self._compile_test_policy("cel_policy.yaml")
    self.assertTrue(result.is_valid, result.formatted_issues())
    self.assertIsNotNone(result.expression())

  def test_compile_policy_result_outlives_env(self):
    # `_compile_test_policy` drops its only reference to the `cel.Env`.
    result = self._compile_test_policy("cel_policy.yaml")
    gc.collect()
    self.assertTrue(result.is_valid, result.formatted_issues())
    expr = result.expression()
    self.assertIs(expr, result.expression())
    self.assertEqual(
        repr(
            expr.eval(
                data={"spec": test_all_types_pb2.TestAllTypes(single_int32=15)}
            )
        ),
        'optional.of("invalid spec, got single_int32=15, wanted <= 10")',
    )

  def test_compile_policy_eval_match_first_condition(self):
    result = self._compile_test_policy("cel_policy.yaml")
    expr = result.expression()

    res = expr.eval(
        data={"spec": test_all_types_pb2.TestAllTypes(single_int32=15)}
    )
    self.assertEqual(
        repr(res),
        'optional.of("invalid spec, got single_int32=15, wanted <= 10")',
    )

  def test_compile_policy_eval_match_second_condition(self):
    result = self._compile_test_policy("cel_policy.yaml")
    expr = result.expression()

    res = expr.eval(
        data={
            "spec": test_all_types_pb2.TestAllTypes(
                single_int32=5,
                standalone_enum=test_all_types_pb2.TestAllTypes.NestedEnum.BAR,
            )
        }
    )
    self.assertEqual(
        repr(res),
        'optional.of("invalid spec, reference to BAR is not allowed")',
    )

  def test_compile_policy_eval_fallthrough(self):
    result = self._compile_test_policy("cel_policy.yaml")
    expr = result.expression()

    spec = test_all_types_pb2.TestAllTypes(
        single_int32=5,
        standalone_enum=test_all_types_pb2.TestAllTypes.NestedEnum.FOO,
    )
    res = expr.eval(data={"spec": spec})
    self.assertEqual(repr(res), "optional.none()")

  def test_rule_is_aggregate_property(self):
    custom_rule = cel_policy.Rule()
    self.assertFalse(custom_rule.is_aggregate)
    custom_rule.is_aggregate = True
    self.assertTrue(custom_rule.is_aggregate)

  def test_parse_and_compile_aggregate_policy(self):
    yaml_content = """\
name: aggregate_policy
rule:
  aggregate:
    - condition: "x > 0"
      output: "'positive'"
    - condition: "x % 2 == 0"
      output: "'even'"
"""
    result = cel_policy.parse_yaml_cel_policy(yaml_content)
    self.assertTrue(result.is_valid, result.formatted_issues())
    policy = result.policy()
    self.assertTrue(policy.rule.is_aggregate)

    env = cel.NewEnv(
        variables={"x": cel.Type.INT},
        extensions=[ext_bindings.ExtBindings(), ext_optional.ExtOptional()],
    )
    compiled = cel_policy.compile(env, policy)
    self.assertTrue(compiled.is_valid, compiled.formatted_issues())
    expr = compiled.expression()
    # Aggregate rules collect every matching output into a list.
    self.assertEqual(expr.eval(data={"x": 4}).value(), ["positive", "even"])
    self.assertEqual(expr.eval(data={"x": -2}).value(), ["even"])
    self.assertEqual(expr.eval(data={"x": -1}).value(), [])

    # Mutating is_aggregate on policy.rule updates the policy in place and
    # switches compilation back to first-match semantics (returning an optional
    # of the first matching output instead of a list of all matching outputs).
    policy.rule.is_aggregate = False
    first_match_compiled = cel_policy.compile(env, policy)
    self.assertTrue(
        first_match_compiled.is_valid, first_match_compiled.formatted_issues()
    )
    self.assertEqual(
        repr(first_match_compiled.expression().eval(data={"x": 4})),
        'optional.of("positive")',
    )


def load_policy_yaml(filename: str) -> str:
  r = runfiles.Create()
  path = r.Rlocation(f"{_TESTDATA_FILE_PATH}/{filename}")
  with open(path, "rt") as f:
    return f.read()


if __name__ == "__main__":
  absltest.main()
