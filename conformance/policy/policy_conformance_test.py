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

"""Conformance tests for cel-python policy."""

import os
from typing import Any
import unittest

from google.protobuf import descriptor_pool
from google.protobuf import message_factory
from google.protobuf import text_format
import yaml

from cel.expr import value_pb2 as value_pb
from absl.testing import absltest
from python.runfiles import runfiles
from cel_expr_python import cel
from cel_expr_python.ext import ext_bindings
from cel_expr_python.ext import ext_optional
from cel_expr_python.policy import cel_policy
from cel.expr.conformance.proto3 import test_all_types_pb2 as test_all_types_proto3  # pylint: disable=unused-import
from cel.expr.conformance.test import suite_pb2 as suite_pb


class PolicyConformanceTestSuite(unittest.TestSuite):
  SKIP_TESTS = [
      # TODO(b/506179116): Fix these.
      # Need to add k8s custom yaml parser and mock runtime.
      "k8s",
      # Expects Java reverse-order type mismatch error message (matches C++
      # skip in cel-cpp conformance/policy/BUILD).
      "compile_errors/aggregate_heterogeneous_outputs",
      # Expects "rule creates unreachable outputs" on the exhaustive subrule,
      # whereas C++ reports "match creates unreachable outputs" on the
      # subsequent unreachable match (matches C++ skip in cel-cpp
      # conformance/policy/BUILD).
      "compile_errors/unreachable_under_unconditional_aggregate_subrule",
  ]

  def __init__(self):
    super().__init__(self)
    self._load_tests()

  def _load_tests(self):
    r = runfiles.Create()
    env_test_files = os.environ.get("TEST_FILES")
    if not env_test_files:
      raise ValueError("TEST_FILES environment variable not set")

    testdata_dir = os.environ.get("TESTDATA_DIR")
    if not testdata_dir:
      raise ValueError("TESTDATA_DIR environment variable not set")

    subdirs = []
    for rel_path in env_test_files.split(testdata_dir):
      rel_path = rel_path.strip()
      if rel_path and rel_path.endswith("policy.yaml"):
        f = testdata_dir + rel_path
        location = r.Rlocation(f)
        if not location:
          raise ValueError(f"Could not resolve rlocation: {f}")
        subdirs.append(os.path.dirname(location))

    subdirs.sort()

    if not subdirs:
      return

    testdata_path = subdirs[0]
    while not testdata_path.endswith("testdata"):
      parent = os.path.dirname(testdata_path)
      if parent == testdata_path:
        raise ValueError("Could not resolve testdata directory")
      testdata_path = parent

    for root in subdirs:
      rel_path = os.path.relpath(root, testdata_path)
      suite_name = rel_path.replace("\\", "/")

      should_skip_suite = any(
          suite_name == pattern or suite_name.startswith(pattern + "/")
          for pattern in self.SKIP_TESTS
      )
      if should_skip_suite:
        print(f"[ SKIPPED SUITE ] {suite_name}")
        continue

      policy_path = os.path.join(root, "policy.yaml")
      with open(policy_path, "rt") as f:
        policy_content = f.read()

      config_path = os.path.join(root, "config.yaml")
      config_content = ""
      context_msg_type_name = None
      if os.path.exists(config_path):
        with open(config_path, "rt") as f:
          config_content = f.read()
        env_cfg = cel.NewEnvConfigFromYaml(config_content)
        context_msg_type_name = (
            env_cfg.context_type if env_cfg.context_type else None
        )

      tests_path = os.path.join(root, "tests.yaml")
      if os.path.exists(tests_path):
        with open(tests_path, "rt") as f:
          self._load_yaml_tests(
              suite_name,
              f.read(),
              policy_content,
              config_content,
              context_msg_type_name,
          )
      else:
        tests_path = os.path.join(root, "tests.textproto")
        if os.path.exists(tests_path):
          with open(tests_path, "rt") as f:
            self._load_textproto_tests(
                suite_name,
                f.read(),
                policy_content,
                config_content,
                context_msg_type_name,
            )

  def _load_yaml_tests(
      self,
      suite_name: str,
      tests_content: str,
      policy_content: str,
      config_content: str,
      context_msg_type_name: str | None,
  ):
    test_suite = yaml.safe_load(tests_content)
    sections = test_suite.get("sections", test_suite.get("section", []))
    for section in sections:
      section_name = section.get("name", "")
      tests = section.get("tests", section.get("test", []))
      for test in tests:
        test_name = test.get("name", "")
        full_test_name = f"{suite_name}/{section_name}/{test_name}"

        context_expr = test.get("context_expr")

        inputs = {}
        for var_name, val_spec in test.get("input", {}).items():
          if isinstance(val_spec, dict) and "expr" in val_spec:
            inputs[var_name] = ("expr", val_spec["expr"])
          elif isinstance(val_spec, dict) and "value" in val_spec:
            inputs[var_name] = ("value", val_spec["value"])
          else:
            inputs[var_name] = ("value", val_spec)

        output_spec = test.get("output")
        if isinstance(output_spec, dict):
          if "expr" in output_spec:
            expected = ("expr", output_spec["expr"])
          elif "value" in output_spec:
            expected = ("value", output_spec["value"])
          elif "error" in output_spec:
            expected = ("errors", [output_spec["error"]])
          elif "error_set" in output_spec:
            expected = ("errors", output_spec["error_set"])
          elif "unknown" in output_spec:
            expected = ("unknown", output_spec["unknown"])
          else:
            expected = ("value", output_spec)
        else:
          expected = ("value", output_spec)

        test_spec = {
            "full_test_name": full_test_name,
            "context_expr": context_expr,
            "inputs": inputs,
            "expected": expected,
        }
        self.addTest(
            PolicyConformanceTest(
                full_test_name,
                suite_name,
                policy_content,
                config_content,
                context_msg_type_name,
                test_spec,
            )
        )

  def _load_textproto_tests(
      self,
      suite_name: str,
      tests_content: str,
      policy_content: str,
      config_content: str,
      context_msg_type_name: str | None,
  ):
    test_suite = suite_pb.TestSuite()
    text_format.Parse(tests_content, test_suite)
    for section in test_suite.sections:
      section_name = section.name
      for test in section.tests:
        test_name = test.name
        full_test_name = f"{suite_name}/{section_name}/{test_name}"

        context_expr = None
        if test.HasField("input_context"):
          if test.input_context.HasField("context_expr"):
            context_expr = test.input_context.context_expr

        inputs = {}
        for var_name, input_val in test.input.items():
          if input_val.HasField("expr"):
            inputs[var_name] = ("expr", input_val.expr)
          elif input_val.HasField("value"):
            inputs[var_name] = ("proto_value", input_val.value)

        if test.HasField("output"):
          out_proto = test.output
          if out_proto.HasField("result_expr"):
            expected = ("expr", out_proto.result_expr)
          elif out_proto.HasField("result_value"):
            expected = ("proto_value", out_proto.result_value)
          elif out_proto.HasField("eval_error"):
            expected = (
                "errors",
                [err.message for err in out_proto.eval_error.errors],
            )
          elif out_proto.HasField("unknown"):
            expected = ("unknown", list(out_proto.unknown.exprs))
          else:
            expected = ("value", True)
        else:
          expected = ("value", True)

        test_spec = {
            "full_test_name": full_test_name,
            "context_expr": context_expr,
            "inputs": inputs,
            "expected": expected,
        }
        self.addTest(
            PolicyConformanceTest(
                full_test_name,
                suite_name,
                policy_content,
                config_content,
                context_msg_type_name,
                test_spec,
            )
        )


class PolicyConformanceTest(absltest.TestCase):

  def __init__(
      self,
      test_name: str,
      suite_name: str,
      policy_content: str,
      config_content: str,
      _context_msg_type_name: str | None,
      test_spec: dict[str, Any],
  ):
    self.descriptor_pool = descriptor_pool.Default()

    # CEL Environment for evaluating input expressions and expected results.
    # Note: this is different from the CEL environment used for compiling the
    # policy (i.e. `env` below).
    self.test_env = cel.NewEnv(
        descriptor_pool=self.descriptor_pool,
        variables={"val": cel.Type.DYN},
        extensions=[
            ext_optional.ExtOptional(),
        ],
    )

    def _function():
      self._run_policy_test(
          suite_name, policy_content, config_content, test_spec
      )

    setattr(self, test_name, _function)
    super().__init__(test_name)

  def _convert_value(self, value: value_pb.Value) -> Any:
    if value.HasField("null_value"):
      return None
    elif value.HasField("bool_value"):
      return value.bool_value
    elif value.HasField("int64_value"):
      return value.int64_value
    elif value.HasField("uint64_value"):
      return value.uint64_value
    elif value.HasField("double_value"):
      return value.double_value
    elif value.HasField("string_value"):
      return value.string_value
    elif value.HasField("bytes_value"):
      return value.bytes_value
    elif value.HasField("object_value"):
      desc = self.descriptor_pool.FindMessageTypeByName(
          value.object_value.TypeName()
      )
      proto = message_factory.GetMessageClass(desc)()
      value.object_value.Unpack(proto)
      return proto
    elif value.HasField("list_value"):
      return [self._convert_value(v) for v in value.list_value.values]
    elif value.HasField("map_value"):
      return {
          self._convert_value(e.key): self._convert_value(e.value)
          for e in value.map_value.entries
      }
    elif value.HasField("enum_value"):
      return value.enum_value
    else:
      self.fail("Unsupported value type: " + str(value))

  def _run_policy_test(
      self,
      suite_name: str,
      policy_content: str,
      config_content: str,
      test_spec: dict[str, Any],
  ):
    parse_result = cel_policy.parse_yaml_cel_policy(policy_content)
    if not parse_result.is_valid:
      self.fail(
          "Failed to parse policy.yaml in"
          f" {suite_name}\nIssues:\n{parse_result.formatted_issues()}"
      )
    policy = parse_result.policy()

    policy_env = cel.NewEnv(
        descriptor_pool=self.descriptor_pool,
        config=cel.NewEnvConfigFromYaml(config_content),
        extensions=[
            ext_bindings.ExtBindings(),
            ext_optional.ExtOptional(),
        ],
        function_impls={
            "locationCode_string": location_code_impl,
            "hasCreditCard": has_credit_card_impl,
            "hasEmailOrPhone": has_email_or_phone_impl,
        },
    )

    compile_result = cel_policy.compile(policy_env, policy)

    expected_type, expected_data = test_spec["expected"]

    if expected_type == "errors":
      self.assertFalse(
          compile_result.is_valid,
          f"Expected compilation to fail in {test_spec['full_test_name']}",
      )
      err_msg = compile_result.formatted_issues()
      for expected_err in expected_data:
        self.assertIn(
            expected_err,
            err_msg,
            "Did not find expected compile time error in"
            f" {test_spec['full_test_name']}",
        )
      return

    self.assertTrue(
        compile_result.is_valid,
        f"Compilation has validation errors in {test_spec['full_test_name']}:"
        f" {compile_result.formatted_issues()}",
    )
    program = compile_result.expression()

    act_data = {}
    if test_spec["context_expr"]:
      context_res = self.test_env.compile(test_spec["context_expr"]).eval()
      proto_obj = context_res.plain_value()
      if hasattr(proto_obj, "DESCRIPTOR"):
        for field in proto_obj.DESCRIPTOR.fields:
          val = getattr(proto_obj, field.name)
          if field.is_repeated:
            if field.message_type and field.message_type.GetOptions().map_entry:
              val = dict(val)
            else:
              val = list(val)
          act_data[field.name] = val
    else:
      for var_name, (in_type, in_val) in test_spec["inputs"].items():
        if in_type == "expr":
          res = self.test_env.compile(in_val).eval()
          act_data[var_name] = res.plain_value()
        elif in_type == "proto_value":
          act_data[var_name] = self._convert_value(in_val)
        else:
          act_data[var_name] = in_val

    activation = policy_env.Activation(act_data)
    actual_res = program.eval(activation)
    actual_repr = repr(actual_res)

    if expected_type == "expr":
      expected_res = self.test_env.compile(expected_data).eval()
      expected_repr = repr(expected_res)
    elif expected_type == "proto_value":
      plain_val = self._convert_value(expected_data)
      expected_res = self.test_env.compile("val").eval(
          self.test_env.Activation({"val": plain_val})
      )
      expected_repr = repr(expected_res)
    else:
      expected_res = self.test_env.compile("val").eval(
          self.test_env.Activation({"val": expected_data})
      )
      expected_repr = repr(expected_res)

    if actual_repr.startswith("optional.of(") and actual_repr.endswith(")"):
      if not expected_repr.startswith("optional."):
        actual_repr = actual_repr[len("optional.of(") : -1]

    self.assertEqual(
        actual_repr,
        expected_repr,
        f"Test failed: {test_spec['full_test_name']}",
    )


def location_code_impl(ip: str) -> str:
  if ip == "10.0.0.1":
    return "us"
  if ip == "10.0.0.2":
    return "de"
  return "ir"


def has_credit_card_impl(arg: Any) -> bool:
  return isinstance(arg, dict) and "cc" in arg


def has_email_or_phone_impl(arg: Any) -> bool:
  return isinstance(arg, dict) and ("email" in arg or "phone" in arg)


if __name__ == "__main__":
  absltest.main(defaultTest="PolicyConformanceTestSuite")
