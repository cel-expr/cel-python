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

"""Basic tests for cel-python."""

import unittest
from py_cel import py_cel
from py_cel.ext import ext_math


class BasicTest(unittest.TestCase):

  def test_basic_test(self):
    env = py_cel.NewEnv(extensions=[ext_math.ExtMath()])
    expr = env.compile("math.sqrt(2)")
    res = expr.eval()
    self.assertAlmostEqual(res.value(), 1.4142135623730951)


if __name__ == "__main__":
  unittest.main()
