# Copyright 2025 Google LLC
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

import datetime
import gc
import importlib
import importlib.abc
import sys

from google.protobuf import duration_pb2 as duration_pb
from google.protobuf import timestamp_pb2 as timestamp_pb
from absl.testing import absltest
from cel_expr_python import cel
from cel.expr.conformance.proto2 import test_all_types_pb2 as test_all_types_pb


class CelTest(absltest.TestCase):

  def setUp(self):
    super().setUp()

    self.env = cel.NewEnv(
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
    self.object_counts_before_test = self._grab_object_counts()

  def tearDown(self):
    """Tears down the test environment."""
    super().tearDown()

    gc.collect()
    # Assert that all Arenas have been garbage-collected
    self.assertEqual(cel._InternalArena._get_instance_count(), 0)
    self._check_for_leaks()

  def _grab_object_counts(self):
    gc.collect()
    all_objects = gc.get_objects()
    type_counts = {}
    for obj in all_objects:
      obj_type = type(obj)
      type_counts[obj_type.__name__] = type_counts.get(obj_type, 0) + 1
    return type_counts

  def _check_for_leaks(self):
    type_counts = self._grab_object_counts()
    for key, count in type_counts.items():
      if count != self.object_counts_before_test.get(key, 0):
        self.fail(
            f"Object count for {key} did not match expected count. "
            f"Expected: {self.object_counts_before_test.get(key, 0)}, "
            f"Actual: {count}",
        )

  def _eval(
      self,
      expression: str,
      data: dict[str, object] = None,
      expected_return_type: cel.Type = None,
  ):
    if data is None:
      data = {}
    expr = self.env.compile(expression)
    if expected_return_type is not None:
      self.assertTrue(
          expected_return_type.is_assignable_from(expr.return_type())
      )
    act = self.env.Activation(data)
    return expr.eval(act)

  def testUnsetVar(self):
    res = self._eval("var_bool", {})
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        'No value with name "var_bool" found in Activation', res.value()
    )

  def testEvalNull(self):
    res = self._eval("null", expected_return_type=cel.Type.NULL)
    self.assertEqual(res.type(), cel.Type.NULL)
    self.assertIsNone(res.value())
    self.assertIsNone(res.plain_value())

    # Non-idiomatic use of None to represent CEL null value.
    # We are doing this just to test that the value is properly evaluated.
    res = self._eval(
        "var_dyn", {"var_dyn": None}, expected_return_type=cel.Type.DYN
    )
    self.assertIsNone(res.value())
    res = self._eval("var_dyn == null", {"var_dyn": None})
    self.assertTrue(res.value())
    res = self._eval("type(var_dyn) == null_type", {"var_dyn": None})
    self.assertTrue(res.value())

  def testEvalBool(self):
    res = self._eval("true", expected_return_type=cel.Type.BOOL)
    self.assertEqual(res.type(), cel.Type.BOOL)
    self.assertEqual(res.value(), True)
    self.assertEqual(res.plain_value(), True)
    res = self._eval("false")
    self.assertEqual(res.value(), False)
    res = self._eval("1 + 1 == 2")
    self.assertEqual(res.value(), True)
    res = self._eval(
        "var_bool", {"var_bool": True}, expected_return_type=cel.Type.BOOL
    )
    self.assertEqual(res.type(), cel.Type.BOOL)
    self.assertEqual(res.value(), True)
    res = self._eval("var_bool", {"var_bool": False})
    self.assertEqual(res.type(), cel.Type.BOOL)
    self.assertEqual(res.value(), False)

  def testEvalInt64(self):
    res = self._eval("42", expected_return_type=cel.Type.INT)
    self.assertEqual(res.type(), cel.Type.INT)
    self.assertEqual(res.value(), 42)
    self.assertEqual(res.plain_value(), 42)
    res = self._eval("1000 + 200 + 30 + 4")
    self.assertEqual(res.value(), 1234)
    res = self._eval(
        "var_int", {"var_int": 42}, expected_return_type=cel.Type.INT
    )
    self.assertEqual(res.type(), cel.Type.INT)
    self.assertEqual(res.value(), 42)
    res = self._eval("var_int / 2", {"var_int": 42})
    self.assertEqual(res.type(), cel.Type.INT)
    self.assertEqual(res.value(), 21)
    res = self._eval("var_int", {"var_int": CompatibleNumber("42")})
    self.assertEqual(res.type(), cel.Type.INT)
    self.assertEqual(res.value(), 42)
    res = self._eval("var_int", {"var_int": 1000000000000000000000000})
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn("int too large", res.value())
    res = self._eval("var_int - 2", {"var_int": 3.14})
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        "'float' object cannot be interpreted as an integer",
        res.value(),
    )
    res = self._eval("var_int", {"var_int": IncompatibleNumber("42")})
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        "'IncompatibleNumber' object cannot be interpreted as an integer",
        res.value(),
    )

  def testEvalInt64_OutOfRange(self):
    with self.assertRaises(Exception) as e:
      self._eval("18446744073709551615")
    assert "invalid int literal" in str(e.exception)

  def testEvalUint64(self):
    res = self._eval("42u", expected_return_type=cel.Type.UINT)
    self.assertEqual(res.type(), cel.Type.UINT)
    self.assertEqual(res.value(), 42)
    self.assertEqual(res.plain_value(), 42)
    res = self._eval("18446744073709551615u")
    self.assertEqual(res.value(), 18446744073709551615)
    self.assertEqual(res.plain_value(), 18446744073709551615)
    res = self._eval(
        "var_uint", {"var_uint": 42}, expected_return_type=cel.Type.UINT
    )
    self.assertEqual(res.type(), cel.Type.UINT)
    self.assertEqual(res.value(), 42)
    res = self._eval("var_uint + 1u", {"var_uint": 42})
    self.assertEqual(res.type(), cel.Type.UINT)
    self.assertEqual(res.value(), 43)
    res = self._eval("var_uint + 1u", {"var_uint": -42})
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn("can't convert negative value to unsigned int", res.value())

  def testEvalDouble(self):
    res = self._eval("3.14", expected_return_type=cel.Type.DOUBLE)
    self.assertEqual(res.type(), cel.Type.DOUBLE)
    self.assertAlmostEqual(res.value(), 3.14, places=3)
    self.assertAlmostEqual(res.plain_value(), 3.14, places=3)
    res = self._eval("3.14 / 2.0")
    self.assertAlmostEqual(res.value(), 1.57, places=3)
    res = self._eval(
        "var_double", {"var_double": 42.0}, expected_return_type=cel.Type.DOUBLE
    )
    self.assertEqual(res.type(), cel.Type.DOUBLE)
    self.assertAlmostEqual(res.value(), 42.0, places=3)
    res = self._eval("var_double + 1.0", {"var_double": 42})
    self.assertEqual(res.type(), cel.Type.DOUBLE)
    self.assertEqual(res.value(), 43)
    res = self._eval("var_double/0.0", {"var_double": 42})
    self.assertEqual(res.type(), cel.Type.DOUBLE)
    self.assertEqual(res.value(), float("inf"))  # is infinity

  def testEvalString(self):
    res = self._eval('"world"', expected_return_type=cel.Type.STRING)
    self.assertEqual(res.type(), cel.Type.STRING)
    self.assertEqual(res.value(), "world")
    self.assertEqual(res.plain_value(), "world")
    res = self._eval('"Hello, " + "world!"')
    self.assertEqual(res.value(), "Hello, world!")
    res = self._eval(
        "var_str", {"var_str": "Hello"}, expected_return_type=cel.Type.STRING
    )
    self.assertEqual(res.type(), cel.Type.STRING)
    self.assertEqual(res.value(), "Hello")
    res = self._eval("var_str + ', ' + var_str + '!'", {"var_str": "Hello"})
    self.assertEqual(res.value(), "Hello, Hello!")
    res = self._eval("var_str", {"var_str": 42})
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        "Unexpected value type for 'var_str': int. (Expected STRING)",
        res.value(),
    )

  def testEvalBytes(self):
    res = self._eval("b'abcd'", expected_return_type=cel.Type.BYTES)
    self.assertEqual(res.type(), cel.Type.BYTES)
    self.assertEqual(res.value(), bytearray("abcd", "ascii"))
    self.assertEqual(res.plain_value(), bytearray("abcd", "ascii"))
    res = self._eval(
        "var_bytes",
        {"var_bytes": bytearray(b"Hello!")},
        expected_return_type=cel.Type.BYTES,
    )
    self.assertEqual(res.type(), cel.Type.BYTES)
    self.assertEqual(res.value(), bytearray(b"Hello!"))
    res = self._eval(
        "var_bytes + b' ' + var_bytes", {"var_bytes": bytearray(b"Hello!")}
    )
    self.assertEqual(res.type(), cel.Type.BYTES)
    self.assertEqual(res.value(), bytearray(b"Hello! Hello!"))

  def testProto(self):
    res = self._eval(
        "cel.expr.conformance.proto2.TestAllTypes{"
        "  single_string: 'hello', "
        "  single_uint64: 42u, "
        "  standalone_message: "
        "    cel.expr.conformance.proto2.TestAllTypes.NestedMessage{"
        "      bb: 1 + 2 + 3 + 4"
        "    }"
        "}",
        expected_return_type=cel.Type(
            "cel.expr.conformance.proto2.TestAllTypes"
        ),
    )
    self.assertEqual(
        res.type(), cel.Type("cel.expr.conformance.proto2.TestAllTypes")
    )
    msg = res.value()
    self.assertEqual(type(msg).__name__, "TestAllTypes")
    self.assertEqual(msg.single_string, "hello")
    self.assertEqual(msg.single_uint64, 42)
    self.assertEqual(msg.standalone_message.bb, 10)

    self.assertEqual(res.plain_value(), msg)

    msg = test_all_types_pb.TestAllTypes()
    msg.single_string = "Hey"
    msg.single_double = 3.14
    res = self._eval(
        "var_msg",
        {"var_msg": msg},
        expected_return_type=cel.Type(
            "cel.expr.conformance.proto2.TestAllTypes"
        ),
    )
    self.assertEqual(res.type().is_message(), True)
    self.assertTrue(
        cel.Type("cel.expr.conformance.proto2.TestAllTypes").is_assignable_from(
            res.type()
        )
    )
    self.assertEqual(
        res.type().name(), "cel.expr.conformance.proto2.TestAllTypes"
    )
    msg = res.value()
    self.assertEqual(type(msg).__name__, "TestAllTypes")
    self.assertEqual(msg.single_string, "Hey")
    self.assertEqual(msg.single_double, 3.14)

    res = self._eval(
        "var_msg.single_string + ', CEL! You are a piece of ' +"
        " string(var_msg.single_double)",
        {"var_msg": msg},
    )
    self.assertEqual(res.value(), "Hey, CEL! You are a piece of 3.14")

  def testProto_unexpectedType(self):
    msg = test_all_types_pb.TestAllTypes.NestedMessage
    res = self._eval("var_msg", {"var_msg": msg})
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertRegex(
        res.value(),
        r"Unexpected value type for 'var_msg':"
        r" .*. \(Expected cel.expr.conformance.proto2.TestAllTypes\)",
    )

  def testEvalList(self):
    res = self._eval("[1, 'CEL', true]", expected_return_type=cel.Type.LIST)
    self.assertEqual(res.type(), cel.Type.LIST)
    self.assertEqual([item.value() for item in res.value()], [1, "CEL", True])
    self.assertEqual(res.plain_value(), [1, "CEL", True])

    res = self._eval("[1]", expected_return_type=cel.Type.List(cel.Type.INT))
    self.assertEqual(res.type(), cel.Type.LIST)
    self.assertEqual(res.value()[0].value(), 1)

    res = self._eval(
        "["
        "cel.expr.conformance.proto2.TestAllTypes{single_string: 'pi'},"
        "cel.expr.conformance.proto2.TestAllTypes{single_double: 3.14}"
        "]",
        expected_return_type=cel.Type.List(
            cel.Type("cel.expr.conformance.proto2.TestAllTypes")
        ),
    )
    res_list = res.value()
    self.assertLen(res_list, 2)
    self.assertEqual(res_list[0].value().single_string, "pi")
    self.assertEqual(res_list[1].value().single_double, 3.14)

    res = self._eval("var_string_list", {"var_string_list": ["foo", "bar"]})
    res_list = res.value()
    self.assertLen(res_list, 2)
    self.assertEqual(res_list[0].value(), "foo")
    self.assertEqual(res_list[1].value(), "bar")

    res = self._eval("var_dyn_list", {"var_dyn_list": ["pi", 3.14]})
    self.assertEqual(res.type(), cel.Type.LIST)
    self.assertEqual(res.plain_value(), ["pi", 3.14])
    res_list = res.value()
    self.assertLen(res_list, 2)
    self.assertEqual(res_list[0].type(), cel.Type.STRING)
    self.assertEqual(res_list[0].value(), "pi")
    self.assertEqual(res_list[1].type(), cel.Type.DOUBLE)
    self.assertAlmostEqual(res_list[1].value(), 3.14, places=3)

  def testEvalList_unexpectedType(self):
    res = self._eval("var_string_list", {"var_string_list": ["pi", 3.14]})
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        "Unexpected value type for 'var_string_list[1]': float. (Expected"
        " STRING)",
        res.value(),
    )

  def testEvalMap(self):
    res = self._eval(
        "{777: 'CEL', 42: 'Python'}",
        expected_return_type=cel.Type.Map(cel.Type.INT, cel.Type.STRING),
    )
    self.assertEqual(res.type(), cel.Type.MAP)
    res_map = res.value()
    self.assertLen(res_map, 2)
    self.assertEqual(res_map[777].value(), "CEL")
    self.assertEqual(res_map[42].value(), "Python")
    self.assertEqual(res.plain_value(), {777: "CEL", 42: "Python"})

    res = self._eval(
        "var_int_map",
        {"var_int_map": {777: "CEL", 42: "Python"}},
        expected_return_type=cel.Type.Map(cel.Type.INT, cel.Type.STRING),
    )
    self.assertEqual(res.type(), cel.Type.MAP)
    res_map = res.value()
    self.assertLen(res_map, 2)
    self.assertEqual(res_map[777].value(), "CEL")
    self.assertEqual(res_map[42].value(), "Python")
    self.assertEqual(res.plain_value(), {777: "CEL", 42: "Python"})

    res = self._eval(
        "var_string_map", {"var_string_map": {"CEL": True, "Python": 42}}
    )
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        "Unexpected value type for 'var_string_map['Python'] -> 42': int."
        " (Expected BOOL)",
        res.value(),
    )

  def testEvalMap_keyTypes(self):
    self.assertEqual(
        cel.Type.Map(cel.Type.INT, cel.Type.STRING).name(), "MAP<INT, STRING>"
    )
    self.assertEqual(
        cel.Type.Map(cel.Type.UINT, cel.Type.DOUBLE).name(), "MAP<UINT, DOUBLE>"
    )
    self.assertEqual(
        cel.Type.Map(cel.Type.BOOL, cel.Type.LIST).name(),
        "MAP<BOOL, LIST<DYN>>",
    )
    self.assertEqual(
        cel.Type.Map(cel.Type.STRING, cel.Type.INT).name(),
        "MAP<STRING, INT>",
    )
    with self.assertRaises(Exception) as e:
      cel.Type.Map(cel.Type("foo.bar.Baz"), cel.Type.STRING)
    self.assertIn("Unsupported map key type: foo.bar.Baz", str(e.exception))

  def testEvalTimestamp(self):
    # google.protobuf.Timestamp
    ts = timestamp_pb.Timestamp()

    res = self._eval(
        "timestamp('2025-01-01T00:00:00Z')",
        expected_return_type=cel.Type.TIMESTAMP,
    )
    self.assertEqual(res.type(), cel.Type.TIMESTAMP)
    self.assertEqual(res.type(), cel.Type("google.protobuf.Timestamp"))
    self.assertEqual(
        res.value(),
        datetime.datetime(2025, 1, 1, tzinfo=datetime.timezone.utc),
    )
    self.assertEqual(
        res.plain_value(),
        datetime.datetime(2025, 1, 1, tzinfo=datetime.timezone.utc),
    )

    res = self._eval(
        "var_timestamp",
        {
            "var_timestamp": datetime.datetime(
                2025, 9, 23, tzinfo=datetime.timezone.utc
            )
        },
        expected_return_type=cel.Type.TIMESTAMP,
    )
    self.assertEqual(res.type(), cel.Type.TIMESTAMP)
    ts.FromDatetime(
        datetime.datetime(2025, 9, 23, tzinfo=datetime.timezone.utc)
    )
    self.assertEqual(
        res.value(),
        datetime.datetime(2025, 9, 23, tzinfo=datetime.timezone.utc),
    )
    self.assertEqual(
        res.plain_value(),
        datetime.datetime(2025, 9, 23, tzinfo=datetime.timezone.utc),
    )

    ts.FromDatetime(
        datetime.datetime(2025, 1, 2, 3, 4, 5, tzinfo=datetime.timezone.utc)
    )
    res = self._eval(
        "var_timestamp",
        {"var_timestamp": ts},
    )
    self.assertEqual(res.type(), cel.Type.TIMESTAMP)
    self.assertEqual(
        res.value(),
        datetime.datetime(2025, 1, 2, 3, 4, 5, tzinfo=datetime.timezone.utc),
    )
    self.assertEqual(
        res.plain_value(),
        datetime.datetime(2025, 1, 2, 3, 4, 5, tzinfo=datetime.timezone.utc),
    )

    res = self._eval(
        "var_timestamp",
        {"var_timestamp": "yesterday"},
    )
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        "Unexpected value type for 'var_timestamp': str. (Expected TIMESTAMP)",
        res.value(),
    )

    ts.seconds = -1000000000000
    res = self._eval(
        "var_timestamp",
        {"var_timestamp": ts},
    )
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        'Timestamp "-29719-04-05T22:13:20Z" below minimum allowed timestamp'
        ' "1-01-01T00:00:00Z"',
        res.value(),
    )

  def testEvalDuration(self):
    # google.protobuf.Duration
    dur = duration_pb.Duration()

    res = self._eval(
        "duration('1m40s')", expected_return_type=cel.Type.DURATION
    )
    self.assertEqual(res.type(), cel.Type.DURATION)
    self.assertEqual(res.type(), cel.Type("google.protobuf.Duration"))
    self.assertEqual(res.value(), datetime.timedelta(seconds=100))
    self.assertEqual(res.plain_value(), datetime.timedelta(seconds=100))

    res = self._eval(
        "var_duration",
        {"var_duration": datetime.timedelta(seconds=999)},
        expected_return_type=cel.Type.DURATION,
    )
    self.assertEqual(res.type(), cel.Type.DURATION)
    self.assertEqual(res.value(), datetime.timedelta(seconds=999))
    self.assertEqual(res.plain_value(), datetime.timedelta(seconds=999))

    dur.FromMilliseconds(123456)
    res = self._eval(
        "var_duration",
        {"var_duration": dur},
    )
    self.assertEqual(res.type(), cel.Type.DURATION)
    self.assertEqual(
        res.value(), datetime.timedelta(seconds=123, microseconds=456000)
    )
    self.assertEqual(
        res.plain_value(), datetime.timedelta(seconds=123, microseconds=456000)
    )

    res = self._eval(
        "var_duration",
        {"var_duration": "forever"},
    )
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        "Unexpected value type for 'var_duration': str. (Expected DURATION)",
        res.value(),
    )

    dur.seconds = 1000000000000
    res = self._eval(
        "var_duration",
        {"var_duration": dur},
    )
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        'Duration "277777777h46m40.456s" above maximum allowed duration '
        '"87660000h0.999999999s"',
        res.value(),
    )

  def testDynType(self):
    msg = test_all_types_pb.TestAllTypes()
    msg.single_string = "Hey"
    msg.single_double = 3.14
    for item in [
        {"value": True, "type": cel.Type.BOOL},
        {"value": 42, "type": cel.Type.INT},
        {"value": 18446744073709551615, "type": cel.Type.UINT},
        {"value": 6.25, "type": cel.Type.DOUBLE},
        {"value": "foo", "type": cel.Type.STRING},
        {"value": b"foo", "type": cel.Type.BYTES},
        {"value": bytearray("bar", "ascii"), "type": cel.Type.BYTES},
        {
            "value": msg,
            "type": cel.Type("cel.expr.conformance.proto2.TestAllTypes"),
        },
    ]:
      v = item["value"]
      t = item["type"]
      res = self._eval("var_dyn", {"var_dyn": v})
      self.assertEqual(
          res.type(),
          t,
          "For value: " + str(v) + " returned value: " + str(res.value()),
      )

    res = self._eval(
        "var_dyn",
        {"var_dyn": -18446744073709551615},
    )
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn("out of range for 'var_dyn'", res.value())

    res = self._eval(
        "var_dyn",
        {"var_dyn": 1000000000000000000000000},
    )
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn("out of range for 'var_dyn'", res.value())

  def testDynType_nonCelType(self):
    res = self._eval("var_dyn", {"var_dyn": self})
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        "Non-CEL value type for 'var_dyn': CelTest",
        res.value(),
    )

  def testTypeType(self):
    res = self._eval(
        "type(1)", expected_return_type=cel.Type.Type(cel.Type.INT)
    )
    self.assertEqual(res.type(), cel.Type.TYPE)
    self.assertEqual(res.value(), cel.Type.INT)

    res = self._eval(
        "type(int)",
        expected_return_type=cel.Type.Type(cel.Type.Type(cel.Type.INT)),
    )
    self.assertEqual(res.type(), cel.Type.TYPE)
    self.assertEqual(res.value(), cel.Type.TYPE)

    # It's ok to leave cel.TYPE unparameterized
    res = self._eval("type(int)", expected_return_type=cel.Type.TYPE)
    self.assertEqual(res.type(), cel.Type.TYPE)
    self.assertEqual(res.value(), cel.Type.TYPE)

    # If the expected type is parameterized, is_assignable_from takes
    # into account the type's parameterization.
    expr = self.env.compile("type(1)")
    self.assertFalse(
        cel.Type.Type(cel.Type.STRING).is_assignable_from(expr.return_type())
    )

    # The following two tests demonstrate a quirk of CEL assignability
    # rules. In the case of type checking, `type(1)` is deemed to be
    # *not* assignable to Type.STRING. However, in runtime use, it would be seen
    # as assignable.
    res = self._eval(
        "var_bool ? type('a') : type(1)",
        {"var_bool": True},
        expected_return_type=cel.Type.Type(cel.Type.STRING),
    )
    self.assertEqual(res.value(), cel.Type.STRING)

    res = self._eval(
        "var_bool ? type('a') : type(1)",
        {"var_bool": False},
        expected_return_type=cel.Type.Type(cel.Type.STRING),
    )
    self.assertEqual(
        res.value(), cel.Type.INT
    )  # This behavior is counterintuitive but works as implemented.

  def testCelExpressionPersistence_checkedExpr(self):
    expr = self.env.compile("var_msg.single_string")
    as_bytes = expr.serialize()

    expr = self.env.deserialize(as_bytes)

    msg = test_all_types_pb.TestAllTypes()
    msg.single_string = "Hey!"
    res = expr.eval(self.env.Activation({"var_msg": msg}))
    self.assertEqual(res.value(), "Hey!")

  def testCelExpressionPersistence_uncheckedExpr(self):
    expr = self.env.compile("runtimely + 2", disable_check=True)
    as_bytes = expr.serialize()

    expr = self.env.deserialize(as_bytes)

    res = expr.eval(self.env.Activation({"runtimely": 40}))
    self.assertEqual(res.value(), 42)

  def testCelExpressionPersistence_badSerializedFormat(self):
    with self.assertRaises(Exception) as e:
      self.env.deserialize("b'foo'")
    self.assertIn("Cannot parse serialized CEL expression", str(e.exception))

  def testCheckedCelExpression_raises(self):
    with self.assertRaises(Exception) as e:
      self.env.compile("runtimely + 2", disable_check=False)
    self.assertIn("undeclared reference to 'runtimely'", str(e.exception))

  def testUncheckedCelExpression(self):
    expr = self.env.compile("runtimely + 2", disable_check=True)
    res = expr.eval(self.env.Activation({"runtimely": 40}))
    self.assertEqual(res.value(), 42)

  def testActivationWithArena(self):
    gc.collect()
    arena = cel.Arena()
    self.assertEqual(cel._InternalArena._get_instance_count(), 1)

    msg = test_all_types_pb.TestAllTypes()
    msg.single_string = "Hey"

    expr = self.env.compile(
        "cel.expr.conformance.proto2.TestAllTypes{"
        "single_string: var_msg.single_string}"
    )

    res = expr.eval(self.env.Activation({"var_msg": msg}, arena=arena))

    # Clear out reference to `arena` to test garbage collection.
    arena = None  # pylint: disable=unused-variable

    # The arena should still be retained by "res"
    gc.collect()
    self.assertEqual(cel._InternalArena._get_instance_count(), 1)

    # Continue using `res` - the underlying arena is still available
    self.assertEqual(res.value().single_string, "Hey")

    # `res` was the last thing holding onto `arena`. Clearing it should
    # also release the Arena itself
    res = None  # pylint: disable=unused-variable

    gc.collect()
    self.assertEqual(cel._InternalArena._get_instance_count(), 0)

  def testImplicitActivation(self):
    expr = self.env.compile("'Hello, ' + var_str")
    res = expr.eval(data={"var_str": "World!"})
    self.assertEqual(res.value(), "Hello, World!")

  def testActivationAndOtherArgs(self):
    expr = self.env.compile("'Hello, ' + var_str")
    with self.assertRaises(Exception) as e:
      expr.eval(
          self.env.Activation(data={"var_str": "World!"}),
          data={"var_str": "World!"},
      )
    self.assertIn(
        "Cannot provide both activation and any other arguments",
        str(e.exception),
    )

  def testCompilationErrorHandling(self):
    # Check parser error.
    with self.assertRaises(Exception) as e:
      self.env.compile("'Hello,' # 'World!'", disable_check=True)
    self.assertIn(
        "1:10: Syntax error: token recognition error at: '#'\n "
        "| 'Hello,' # 'World!'\n "
        "| .........^",
        str(e.exception),
    )
    self.assertIn(
        "1:12: Syntax error: extraneous input ''World!'' expecting <EOF>\n "
        "| 'Hello,' # 'World!'\n "
        "| ...........^",
        str(e.exception),
    )

    # Check type-checker error.
    with self.assertRaises(Exception) as e:
      self.env.compile("'Hello,' - 'World!'")
    self.assertIn(
        "<input>:1:10: found no matching overload for '_-_' applied to"
        " '(string, string)'\n "
        "| 'Hello,' - 'World!'\n "
        "| .........^",
        str(e.exception),
    )

  def testErrorHandling(self):
    bad_env = cel.NewEnv(_BadDescriptorPool(), variables={})
    with self.assertRaises(Exception) as e:
      bad_env.compile("cel.expr.conformance.proto2.TestSomeTypes{}")
    self.assertRegex(
        str(e.exception),
        r"Could not find file containing symbol:.* \[NOT_FOUND\]",
    )


class CompatibleNumber:

  def __init__(self, value: str):
    self.value = value

  # Defining __index__ makes this a number type from Python's perspective
  # and also a valid integer for CEL.
  def __index__(self):
    return int(self.value)


class IncompatibleNumber:

  def __init__(self, value: str):
    self.value = value

  # Defining __int__ makes this a number type from Python's perspective,
  # but without at least __index__, it won't be a valid integer for CEL.
  def __int__(self):
    return len(self.value)


class _BadDescriptorPool:

  def FindFileContainingSymbol(self, symbol_name: str):  # pylint: disable=invalid-name
    raise LookupError("Could not find file containing symbol: %s" % symbol_name)


class CelWithoutProtoSupportTest(absltest.TestCase):
  """Test that the environment can be created without proto support."""

  def setUp(self):
    super().setUp()
    self.msg = test_all_types_pb.TestAllTypes()
    self.msg.single_string = "Hey"

    # "Unimport" any google.protobuf modules if they are already imported.
    for module_name in list(sys.modules):
      if module_name.startswith("google.protobuf"):
        del sys.modules[module_name]

    # Make it impossible to import descriptor_pool.
    class UnluckyFinder(importlib.abc.MetaPathFinder):

      def find_spec(self, fullname, unused_path, unused_target=None):
        if fullname.startswith("google.protobuf."):
          raise ImportError("Not found")
        return None

    sys.meta_path.insert(0, UnluckyFinder())

  def tearDown(self):
    # Remove the unlucky finder from the meta path.
    sys.meta_path.pop(0)
    super().tearDown()

  def testEvalWithNonProtoTypes(self):
    cel_env = cel.NewEnv(
        descriptor_pool=None,
        variables={
            "var_str": cel.Type.STRING,
            "var_map": cel.Type.Map(cel.Type.STRING, cel.Type.STRING),
            "var_list": cel.Type.List(cel.Type.STRING),
        },
    )
    data = {
        "var_str": "foo",
        "var_map": {"key": "bar"},
        "var_list": ["foo", "bar", "baz"],
    }
    res = cel_env.compile("var_str").eval(data=data)
    self.assertEqual(res.value(), "foo")

    res = cel_env.compile("var_map['key']").eval(data=data)
    self.assertEqual(res.value(), "bar")

    res = cel_env.compile("var_list[2]").eval(data=data)
    self.assertEqual(res.value(), "baz")

  def testErrorOnProtoAccess(self):
    cel_env = cel.NewEnv(
        descriptor_pool=None,
        variables={
            "var_proto": cel.Type.DYN,
        },
    )
    res = cel_env.compile("var_proto.single_string").eval(
        data={"var_proto": self.msg}
    )
    self.assertEqual(res.type(), cel.Type.ERROR)
    self.assertIn(
        "Descriptor not found for message type"
        " 'cel.expr.conformance.proto2.TestAllTypes'",
        str(res.value()),
    )

    with self.assertRaises(Exception) as e:
      cel_env.compile(
          "cel.expr.conformance.proto2.TestAllTypes{single_string: 'hello'}"
      ).eval()
    self.assertIn(
        "undeclared reference to 'cel.expr.conformance.proto2.TestAllTypes'",
        str(e.exception),
    )

  def testErrorOnProtoCreation(self):
    cel_env = cel.NewEnv(
        descriptor_pool=None,
        variables={
            "var_proto": cel.Type.DYN,
        },
    )
    # Disable type checking to allow the compilation to succeed.
    expr = cel_env.compile(
        "cel.expr.conformance.proto2.TestAllTypes{single_string: 'hello'}",
        disable_check=True,
    )

    with self.assertRaises(Exception) as e:
      expr.eval()
    self.assertIn(
        "Invalid struct creation: missing type info for"
        " 'cel.expr.conformance.proto2.TestAllTypes'",
        str(e.exception),
    )


if __name__ == "__main__":
  absltest.main()
