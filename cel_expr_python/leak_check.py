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

"""Reference and memory leak detection helpers for CEL Python tests."""

import array
import collections.abc
import functools
import gc
import sys
import tracemalloc
from typing import Any, TypeVar

_DEFAULT_WARMUPS = 2
_DEFAULT_RUNS = 3

_F = TypeVar("_F", bound=collections.abc.Callable[..., Any])
_T = TypeVar("_T", bound=type)


def _live_blocks() -> int:
  """Returns the allocated Python memory block count after clearing caches."""
  # Clear CPython's internal caches (e.g., the type attribute lookup cache)
  # before sampling so one-time cached entries are not mistaken for leaks.
  clear_caches = getattr(sys, "_clear_internal_caches", None) or getattr(
      sys, "_clear_type_cache", None
  )
  if clear_caches is not None:
    clear_caches()
  gc.collect()
  return sys.getallocatedblocks()


def _tracemalloc_report(
    run_once: collections.abc.Callable[[], None],
    iterations: int = _DEFAULT_RUNS,
    limit: int = 5,
) -> str:
  """Re-runs `run_once` under tracemalloc and formats top growing sites."""
  was_tracing = tracemalloc.is_tracing()
  if not was_tracing:
    tracemalloc.start(10)
  try:
    run_once()
    _live_blocks()
    before = tracemalloc.take_snapshot()
    for _ in range(iterations):
      run_once()
    _live_blocks()
    after = tracemalloc.take_snapshot()
  finally:
    if not was_tracing:
      tracemalloc.stop()
  ignore = [tracemalloc.Filter(False, "<frozen importlib._bootstrap*>")]
  if tracemalloc.__file__:
    ignore.append(tracemalloc.Filter(False, tracemalloc.__file__))
  stats = after.filter_traces(ignore).compare_to(
      before.filter_traces(ignore), "traceback"
  )
  lines = []
  sites = 0
  for stat in stats:
    if stat.count_diff <= 0:
      continue
    lines.append(
        f"  {stat.count_diff:+d} blocks ({stat.size_diff:+d} B) allocated at:"
    )
    lines.extend(
        "    " + line for line in stat.traceback.format(most_recent_first=True)
    )
    sites += 1
    if sites >= limit:
      break
  return "\n".join(lines) or "  (no growth attributed by tracemalloc)"


def assert_no_leaks(
    test_method: _F,
    *,
    warmups: int = _DEFAULT_WARMUPS,
    runs: int = _DEFAULT_RUNS,
) -> _F:
  """Wraps a `unittest.TestCase` method to fail if live blocks grow every run.

  Uses CPython's `regrtest -R` strategy: runs `warmups` unmeasured iterations
  to populate lazy process-wide caches, then `runs` measured iterations,
  failing only if `sys.getallocatedblocks()` increases on every measured run.

  Args:
    test_method: The test method to wrap.
    warmups: Number of warm-up repetitions before measuring.
    runs: Number of measured repetitions.

  Returns:
    The wrapped test method.
  """

  @functools.wraps(test_method)
  def wrapper(self: Any, *args: Any, **kwargs: Any) -> Any:
    total_runs = warmups + runs
    # Store samples in a C array so recording them keeps no Python ints alive.
    blocks = array.array("q", [0]) * (total_runs + 1)
    blocks[0] = _live_blocks()
    for i in range(total_runs):
      test_method(self, *args, **kwargs)
      blocks[i + 1] = _live_blocks()
    deltas = [blocks[i + 1] - blocks[i] for i in range(warmups, total_runs)]
    if all(d > 0 for d in deltas):
      self.fail(
          f"Memory blocks grew on every one of {runs} repetitions"
          f" (after {warmups} warm-up runs): deltas={deltas}."
          " Top growing allocation sites:\n"
          + _tracemalloc_report(
              lambda: test_method(self, *args, **kwargs), iterations=runs
          )
      )

  return wrapper  # type: ignore[return-value]


def leak_checked(cls: _T) -> _T:
  """Class decorator wrapping every `test*` method with `assert_no_leaks`."""
  for name, attr in list(cls.__dict__.items()):
    if name.startswith("test") and callable(attr):
      setattr(cls, name, assert_no_leaks(attr))
  return cls
