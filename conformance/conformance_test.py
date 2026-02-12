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

"""Conformance tests for cel-python."""

import datetime
import os
import re
import unittest

from google.protobuf import descriptor_pool
from google.protobuf import message_factory
from google.protobuf import text_format

from cel.expr import checked_pb2 as checked_pb
from cel.expr import value_pb2 as value_pb
from absl.testing import absltest
from python.runfiles import runfiles
from cel_expr_python import cel
from cel_expr_python.ext import ext_bindings
from cel_expr_python.ext import ext_encoders
from cel_expr_python.ext import ext_math
from cel_expr_python.ext import ext_optional
from cel_expr_python.ext import ext_proto
from cel_expr_python.ext import ext_string
from cel.expr.conformance.proto2 import test_all_types_extensions_pb2 as test_all_types_extensions_proto2  # pylint: disable=unused-import
from cel.expr.conformance.proto2 import test_all_types_pb2 as test_all_types_proto2  # pylint: disable=unused-import
from cel.expr.conformance.proto3 import test_all_types_pb2 as test_all_types_proto3  # pylint: disable=unused-import
from cel.expr.conformance.test import simple_pb2 as simple_pb


class ConformanceTestSuite(unittest.TestSuite):

  SKIP_TESTS = [
      # TODO(b/452657003): add support for extension functions in Python.
      "type_deduction/functions/function_result_type",
      "type_deduction/type_parameters/multiple_parameters_generality",
      "type_deduction/type_parameters/multiple_parameters_generality_2",
      "type_deduction/type_parameters/multiple_parameters_parameterized_ovl",
      "type_deduction/type_parameters/multiple_parameters_parameterized_ovl_2",
      "type_deduction/flexible_type_parameter_assignment/unconstrained_type_var_as_dyn",
      # TODO(b/178627883): Strong typing support for enums not implemented.
      "enums/strong_proto2/.*",
      "enums/strong_proto3/.*",
      # TODO(b/173160413): Parse-only qualified variable lookup "x.y"
      # with binding "x.y" or "y" within container "x" fails
      "fields/qualified_identifier_resolution/qualified_identifier_resolution_unchecked",
      "fields/qualified_identifier_resolution/map_value_repeat_key_heterogeneous",
      "namespace/namespace/self_eval_container_lookup_unchecked",
      # TODO(b/453051120): fix support for FieldMask->toJson
      "wrappers/field_mask/to_json",
      # TODO(b/464071224): fix support for triple-quoted unescaped punctuation.
      "parse/bytes_literals/triple_double_quoted_unescaped_punctuation",
      "parse/bytes_literals/triple_single_quoted_unescaped_punctuation",
      "parse/string_literals/triple_double_quoted_unescaped_punctuation",
      "parse/string_literals/triple_single_quoted_unescaped_punctuation",
      # TODO(b/481818110): fix support for optional types.
      "optionals/optionals/empty_struct_optindex_hasValue",
      "optionals/optionals/optional_empty_struct_optindex_hasValue",
      "optionals/optionals/optional_struct_optindex_index_value",
      "optionals/optionals/optional_struct_optindex_index_value",
      "optionals/optionals/optional_struct_optindex_value",
      "optionals/optionals/struct_optindex_value",
  ]

  def __init__(self):
    super().__init__(self)
    self._load_tests()

  def _load_tests(self):
    """Loads all tests from a collection of test files.

    Test files are structured as follows:
    - testdata/
      - file1.textproto
        - section1/
          - test1.textproto
          - test2.textproto
        - section2/
          - test1.textproto
          - test2.textproto
      - file2.textproto
        ....
    This function names tests as "file_name/section_name/test_name", and adds
    them to the test suite.
    """
    testfiles = self._all_test_files(
        "cel-spec+/tests/simple/testdata"
    )

    for testfile in testfiles:
      with open(testfile, "rb", encoding=None, errors=None) as data_file:
        data = data_file.read()
      test_file = simple_pb.SimpleTestFile()
      text_format.Parse(data, test_file)
      for section in test_file.section:
        for test in section.test:
          file_name = testfile[testfile.rfind("/") + 1 :].removesuffix(
              ".textproto"
          )
          test_name = file_name + "/" + section.name + "/" + test.name
          should_skip = any(
              re.search(pattern, test_name) for pattern in self.SKIP_TESTS
          )
          if not should_skip:
            self.addTest(ConformanceTest(test_name, test))

  def _all_test_files(self, path: str) -> list[str]:
    r = runfiles.Create()
    location = r.Rlocation(path)
    if not location:
      raise ValueError("Could not resolve path: " + path)

    test_files = []
    for root, _, files in os.walk(location):
      for f in files:
        if f.endswith(".textproto"):
          test_files.append(os.path.join(root, f))
    return sorted(test_files)


class ConformanceTest(absltest.TestCase):

  EXTENSIONS_PER_TESTFILE = {
      "bindings_ext": [ext_bindings.ExtBindings()],
      "encoders_ext": [ext_encoders.ExtEncoders()],
      "math_ext": [ext_math.ExtMath()],
      "optionals": [ext_optional.ExtOptional()],
      "proto2_ext": [ext_proto.ExtProto()],
      "string_ext": [ext_string.ExtString()],
      "type_deduction": [ext_optional.ExtOptional()],
  }

  def __init__(self, test_name: str, simple_test: simple_pb.SimpleTest):
    def _function():
      self._run_conformance_test(simple_test)

    setattr(self, test_name, _function)
    super().__init__(test_name)

  def _run_conformance_test(self, simple_test: simple_pb.SimpleTest):
    decls = {}
    values = {}
    for decl in simple_test.type_env:
      if decl.ident.HasField("type"):
        decls[decl.name] = self._convert_type(decl.ident.type)
      else:
        self.fail("Unsupported declaration type: " + str(decl))

    extensions = []
    for key, value in self.EXTENSIONS_PER_TESTFILE.items():
      if self._testMethodName.find(key) != -1:
        extensions = value
        break

    self.descriptor_pool = descriptor_pool.Default()
    self.env = cel.NewEnv(
        self.descriptor_pool,
        variables=decls,
        extensions=extensions,
        container=simple_test.container,
    )
    try:
      compiled_expr = self.env.compile(
          simple_test.expr, disable_check=simple_test.disable_check
      )
    except Exception as e:  # pylint: disable=broad-except
      # print("EXCEPTION: " + str(e))
      if simple_test.HasField("eval_error"):
        # Error messages in C++ are not the same as those in the conformance
        # tests, so we cannot verify the error message.
        return
      else:
        raise e

    if simple_test.typed_result.HasField("deduced_type"):
      self.assertType(
          compiled_expr.return_type(), simple_test.typed_result.deduced_type
      )

    if simple_test.check_only:
      return

    for key, value in simple_test.bindings.items():
      values[key] = self._convert_value(value.value)

    act = self.env.Activation(values)
    try:
      res = compiled_expr.eval(act)
    except Exception as e:  # pylint: disable=broad-except
      if simple_test.HasField("eval_error"):
        # Error messages in C++ are not the same as those in the conformance
        # tests, so we cannot verify the error message.
        return
      else:
        raise e
    self.assertEvalResult(res, simple_test)

  def assertEvalResult(self, result, simple_test: simple_pb.SimpleTest):
    if simple_test.HasField("value"):
      self.assertValue(result, simple_test.value)
    elif simple_test.HasField("eval_error"):
      if len(simple_test.eval_error.errors) > 1:
        self.fail(
            "Multiple errors not supported: " + str(simple_test.eval_error)
        )
      self.assertEqual(result.type(), cel.Type.ERROR)
      # At this point, error messages from the C++ side are not the same as
      # those in the conformance tests.
      # self.assertIn(
      #       simple_test.eval_error.errors[0].message, result.value())
    elif simple_test.HasField("typed_result"):
      self.assertValue(result, simple_test.typed_result.result)
    else:
      self.assertEqual(result.value(), True)

  def _convert_primitive_type(self, primitive: checked_pb.Type.PrimitiveType):
    if primitive == checked_pb.Type.PrimitiveType.BOOL:
      return cel.Type.BOOL
    elif primitive == checked_pb.Type.PrimitiveType.INT64:
      return cel.Type.INT
    elif primitive == checked_pb.Type.PrimitiveType.UINT64:
      return cel.Type.UINT
    elif primitive == checked_pb.Type.PrimitiveType.DOUBLE:
      return cel.Type.DOUBLE
    elif primitive == checked_pb.Type.PrimitiveType.STRING:
      return cel.Type.STRING
    elif primitive == checked_pb.Type.PrimitiveType.BYTES:
      return cel.Type.BYTES
    else:
      self.fail("Unsupported primitive type: " + str(primitive))

  def _convert_type(self, tp: checked_pb.Type):
    kind = tp.WhichOneof("type_kind")
    if kind == "dyn":
      return cel.Type.DYN
    elif kind == "null":
      return cel.Type.NULL
    elif kind == "primitive":
      return self._convert_primitive_type(tp.primitive)
    elif kind == "wrapper":
      return self._convert_primitive_type(tp.wrapper)
    elif kind == "message_type":
      return cel.Type(tp.message_type)
    elif kind == "list_type":
      return cel.Type.List(self._convert_type(tp.list_type.elem_type))
    elif kind == "map_type":
      return cel.Type.Map(
          self._convert_type(tp.map_type.key_type),
          self._convert_type(tp.map_type.value_type),
      )
    elif kind == "abstract_type":
      return cel.Type.AbstractType(
          tp.abstract_type.name,
          [
              self._convert_type(param)
              for param in tp.abstract_type.parameter_types
          ],
      )
    else:
      self.fail("Unsupported type: " + str(type))

  def _convert_value(self, value: value_pb.Value):
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

  def _type_by_name(self, type_name: str):
    if type_name == "dyn":
      return cel.Type.DYN
    elif type_name == "null_type":
      return cel.Type.NULL
    elif type_name == "bool":
      return cel.Type.BOOL
    elif type_name == "int":
      return cel.Type.INT
    elif type_name == "uint":
      return cel.Type.UINT
    elif type_name == "double":
      return cel.Type.DOUBLE
    elif type_name == "string":
      return cel.Type.STRING
    elif type_name == "bytes":
      return cel.Type.BYTES
    elif type_name == "list":
      return cel.Type.LIST
    elif type_name == "map":
      return cel.Type.MAP
    elif type_name == "timestamp" or type_name == "google.protobuf.Timestamp":
      return cel.Type.TIMESTAMP
    elif type_name == "duration" or type_name == "google.protobuf.Duration":
      return cel.Type.DURATION
    elif type_name == "type":
      return cel.Type.TYPE
    else:
      desc = self.descriptor_pool.FindMessageTypeByName(type_name)
      if desc is not None:
        return cel.Type(type_name)
      desc = self.descriptor_pool.FindEnumTypeByName(type_name)
      if desc is not None:
        return cel.Type(type_name)
      else:
        self.fail("Unsupported type: " + str(type_name))

  def assertValue(self, result: cel.Value, expected_value: value_pb.Value):
    if expected_value.HasField("null_value"):
      self.assertIsNone(result.value())
    elif expected_value.HasField("bool_value"):
      self.assertEqual(result.value(), expected_value.bool_value)
    elif expected_value.HasField("int64_value"):
      self.assertEqual(result.value(), expected_value.int64_value)
    elif expected_value.HasField("uint64_value"):
      self.assertEqual(result.value(), expected_value.uint64_value)
    elif expected_value.HasField("double_value"):
      self.assertEqual(result.value(), expected_value.double_value)
    elif expected_value.HasField("string_value"):
      self.assertEqual(result.value(), expected_value.string_value)
    elif expected_value.HasField("bytes_value"):
      self.assertEqual(result.value(), expected_value.bytes_value)
    elif expected_value.HasField("list_value"):
      actual = result.value()
      expected = expected_value.list_value
      self.assertLen(actual, len(expected.values))
      for i, actual_value in enumerate(actual):
        self.assertValue(actual_value, expected.values[i])
    elif expected_value.HasField("map_value"):
      actual = result.value()
      expected = expected_value.map_value
      self.assertLen(actual, len(expected.entries))
      for entry in expected.entries:
        actual_value = actual[self._convert_value(entry.key)]
        self.assertValue(actual_value, entry.value)
    elif expected_value.HasField("object_value"):
      desc = self.descriptor_pool.FindMessageTypeByName(
          expected_value.object_value.TypeName()
      )
      proto = message_factory.GetMessageClass(desc)()
      expected_value.object_value.Unpack(proto)
      if proto.DESCRIPTOR.full_name == "google.protobuf.Duration":
        self.assertEqual(result.value(), proto.ToTimedelta())
      elif proto.DESCRIPTOR.full_name == "google.protobuf.Timestamp":
        self.assertEqual(
            result.value(),
            proto.ToDatetime().replace(tzinfo=datetime.timezone.utc),
        )
    elif expected_value.HasField("type_value"):
      self.assertEqual(result.type(), cel.Type.TYPE)
      self.assertEqual(
          result.value(), self._type_by_name(expected_value.type_value)
      )
    elif expected_value.HasField("enum_value"):
      self.assertEqual(result.value(), expected_value.enum_value.value)
    else:
      self.fail("Unsupported value type: " + str(expected_value))

  def assertPrimitiveType(
      self,
      result_type: cel.Type,
      expected_primitive: checked_pb.Type.PrimitiveType,
  ):
    if expected_primitive == checked_pb.Type.PrimitiveType.BOOL:
      self.assertEqual(result_type, cel.Type.BOOL)
    elif expected_primitive == checked_pb.Type.PrimitiveType.INT64:
      self.assertEqual(result_type, cel.Type.INT)
    elif expected_primitive == checked_pb.Type.PrimitiveType.UINT64:
      self.assertEqual(result_type, cel.Type.UINT)
    elif expected_primitive == checked_pb.Type.PrimitiveType.DOUBLE:
      self.assertEqual(result_type, cel.Type.DOUBLE)
    elif expected_primitive == checked_pb.Type.PrimitiveType.STRING:
      self.assertEqual(result_type, cel.Type.STRING)
    elif expected_primitive == checked_pb.Type.PrimitiveType.BYTES:
      self.assertEqual(result_type, cel.Type.BYTES)
    else:
      self.fail(
          "Unsupported primitive type validation " + str(expected_primitive)
      )

  def assertType(self, result_type: cel.Type, expected_type: checked_pb.Type):
    kind = expected_type.WhichOneof("type_kind")
    if kind == "primitive":
      self.assertPrimitiveType(result_type, expected_type.primitive)
    elif kind == "wrapper":
      self.assertPrimitiveType(result_type, expected_type.wrapper)
    elif kind == "null":
      self.assertEqual(result_type, cel.Type.NULL)
    elif kind == "list_type":
      self.assertEqual(
          result_type,
          cel.Type.List(self._convert_type(expected_type.list_type.elem_type)),
      )
    elif kind == "map_type":
      self.assertEqual(
          result_type,
          cel.Type.Map(
              self._convert_type(expected_type.map_type.key_type),
              self._convert_type(expected_type.map_type.value_type),
          ),
      )
    elif kind == "message_type":
      self.assertEqual(result_type, cel.Type(expected_type.message_type))
    elif kind == "well_known":
      if expected_type.well_known == checked_pb.Type.WellKnownType.TIMESTAMP:
        self.assertEqual(result_type, cel.Type.TIMESTAMP)
      elif expected_type.well_known == checked_pb.Type.WellKnownType.DURATION:
        self.assertEqual(result_type, cel.Type.DURATION)
    elif kind == "abstract_type":
      self.assertEqual(
          result_type,
          cel.Type.AbstractType(
              expected_type.abstract_type.name,
              [
                  self._convert_type(param)
                  for param in expected_type.abstract_type.parameter_types
              ],
          ),
      )
    else:
      self.fail("Unsupported type validation: " + str(expected_type))


if __name__ == "__main__":
  absltest.main(defaultTest="ConformanceTestSuite")
