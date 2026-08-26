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

"""Multi-threaded tests for cel-python."""

import collections.abc
import concurrent.futures
import dataclasses
import gc
import logging
import time
from typing import Any

from absl.testing import absltest
from cel_expr_python import cel
from cel.expr.conformance.proto2 import test_all_types_pb2 as test_all_types_pb


@dataclasses.dataclass(frozen=True)
class _TestCase:
  expr: str
  data: collections.abc.Callable[[int], dict[str, Any]]
  expected: collections.abc.Callable[[int], Any]


_NUM_EVALUATIONS = 10000
_NUM_COMPILATIONS = 1000

_TEST_MSG = test_all_types_pb.TestAllTypes(single_int64=100)

_TEST_CASES = [
    _TestCase(
        expr="var_int * var_int",
        data=lambda n: {"var_int": n},
        expected=lambda n: n * n,
    ),
    _TestCase(
        expr="var_str + '_' + string(var_int)",
        data=lambda n: {"var_str": "num", "var_int": n},
        expected=lambda n: f"num_{n}",
    ),
    _TestCase(
        expr="var_int % 2 == 0",
        data=lambda n: {"var_int": n},
        expected=lambda n: n % 2 == 0,
    ),
    _TestCase(
        expr="[var_int, var_int + 1, var_int + 2]",
        data=lambda n: {"var_int": n},
        expected=lambda n: [n, n + 1, n + 2],
    ),
    _TestCase(
        expr="var_int_map[var_int]",
        data=lambda n: {"var_int_map": {n: f"val_{n}"}, "var_int": n},
        expected=lambda n: f"val_{n}",
    ),
    _TestCase(
        expr="var_msg.single_int64 + var_int",
        data=lambda n: {"var_msg": _TEST_MSG, "var_int": n},
        expected=lambda n: 100 + n,
    ),
    _TestCase(
        expr=(
            "cel.expr.conformance.proto2.TestAllTypes{"
            "  single_int64: var_int, single_string: var_str"
            "}"
        ),
        data=lambda n: {"var_int": n, "var_str": f"msg_{n}"},
        expected=lambda n: test_all_types_pb.TestAllTypes(
            single_int64=n, single_string=f"msg_{n}"
        ),
    ),
    _TestCase(
        expr="{'key': var_str, 'value': var_int}",
        data=lambda n: {"var_str": f"val_{n}", "var_int": n},
        expected=lambda n: {"key": f"val_{n}", "value": n},
    ),
    _TestCase(
        expr="[var_int, var_int + 1, var_int + 2].all(x, x >= var_int)",
        data=lambda n: {"var_int": n},
        expected=lambda n: True,
    ),
]


class CelParallelTest(absltest.TestCase):

  def setUp(self):
    super().setUp()

    self.env = cel.NewEnv(
        variables={
            "var_int": cel.Type.INT,
            "var_str": cel.Type.STRING,
            "var_int_map": cel.Type.Map(cel.Type.INT, cel.Type.STRING),
            "var_msg": cel.Type("cel.expr.conformance.proto2.TestAllTypes"),
        },
    )
    self.object_counts_before_test = self._grab_object_counts()

  def tearDown(self):
    """Tears down the test environment."""
    super().tearDown()

    gc.collect()
    # Assert that all Arenas have been garbage-collected
    self.assertEqual(cel._InternalArena._get_instance_count(), 0)
    self._check_for_leaks()

  def _grab_object_counts(self) -> dict[str, int]:
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

  def _test_eval(self, multi_threaded: bool):
    compiled_exprs = [self.env.compile(tc.expr) for tc in _TEST_CASES]

    def eval_expr(n: int) -> Any:
      idx = n % len(_TEST_CASES)
      test_case = _TEST_CASES[idx]
      expr = compiled_exprs[idx]
      data = test_case.data(n)
      return expr.eval(data=data).plain_value()

    start_time = time.perf_counter()
    if multi_threaded:
      with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
        results = list(executor.map(eval_expr, range(_NUM_EVALUATIONS)))
    else:
      results = [eval_expr(n) for n in range(_NUM_EVALUATIONS)]
    duration_ms = (time.perf_counter() - start_time) * 1000

    mode = "Multi-threaded" if multi_threaded else "Sequential"
    logging.info("%s evaluation duration: %.2f ms", mode, duration_ms)

    self.assertLen(results, _NUM_EVALUATIONS)
    for i, res in enumerate(results):
      test_case = _TEST_CASES[i % len(_TEST_CASES)]
      self.assertEqual(res, test_case.expected(i))

  def testMultiThreadedEval(self):
    self._test_eval(multi_threaded=True)

  def testSequentialEval(self):
    self._test_eval(multi_threaded=False)

  def _test_compile(self, multi_threaded: bool):
    def compile_expr(n: int) -> cel.Expression:
      test_case = _TEST_CASES[n % len(_TEST_CASES)]
      return self.env.compile(test_case.expr)

    start_time = time.perf_counter()
    if multi_threaded:
      with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
        results = list(executor.map(compile_expr, range(_NUM_COMPILATIONS)))
    else:
      results = [compile_expr(n) for n in range(_NUM_COMPILATIONS)]
    duration_ms = (time.perf_counter() - start_time) * 1000

    mode = "Multi-threaded" if multi_threaded else "Sequential"
    logging.info("%s compilation duration: %.2f ms", mode, duration_ms)

    self.assertLen(results, _NUM_COMPILATIONS)
    for i, expr in enumerate(results):
      test_case = _TEST_CASES[i % len(_TEST_CASES)]
      data = test_case.data(i)
      self.assertEqual(
          expr.eval(data=data).plain_value(), test_case.expected(i)
      )

  def testMultiThreadedCompilation(self):
    self._test_compile(multi_threaded=True)

  def testSequentialCompilation(self):
    self._test_compile(multi_threaded=False)

  def testSharedValueConcurrentAccess(self):
    expr_scalar = self.env.compile("var_int * 2")
    expr_list = self.env.compile("[var_int, var_int + 1, var_int + 2]")
    expr_map = self.env.compile("{'key': var_str, 'value': var_int}")

    def run_concurrent_value_test(n: int):
      val_scalar = expr_scalar.eval(data={"var_int": n})
      val_list = expr_list.eval(data={"var_int": n})
      val_map = expr_map.eval(data={"var_str": f"k_{n}", "var_int": n})

      def read_values(_):
        # Concurrently access plain_value, value, type, and repr
        # on shared instances
        self.assertEqual(val_scalar.plain_value(), n * 2)
        self.assertEqual(val_scalar.value(), n * 2)
        self.assertEqual(val_scalar.type(), cel.Type.INT)
        self.assertNotEmpty(str(val_scalar))

        plain_list = val_list.plain_value()
        self.assertEqual(plain_list, [n, n + 1, n + 2])
        list_accessors = val_list.value()
        for idx, item in enumerate(list_accessors):
          self.assertEqual(item.plain_value(), n + idx)
          self.assertEqual(item.value(), n + idx)
          self.assertEqual(item.type(), cel.Type.INT)
          self.assertNotEmpty(str(item))

        plain_map = val_map.plain_value()
        self.assertEqual(plain_map, {"key": f"k_{n}", "value": n})
        map_accessors = val_map.value()
        self.assertEqual(map_accessors["key"].plain_value(), f"k_{n}")
        self.assertEqual(map_accessors["key"].value(), f"k_{n}")
        self.assertEqual(map_accessors["value"].plain_value(), n)
        self.assertEqual(map_accessors["value"].value(), n)

      with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
        list(executor.map(read_values, range(50)))

    for i in range(10):
      run_concurrent_value_test(i)


if __name__ == "__main__":
  absltest.main()
