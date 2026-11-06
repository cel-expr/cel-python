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

"""Tests that macOS extension modules export only their init function.

Exported copies of the statically linked protobuf and abseil runtimes are
coalesced by dyld with any other copy loaded in the process (for example
ortools'), which crashes at import time.
"""

import subprocess
import sys
import types

from absl.testing import absltest
from absl.testing import parameterized
from cel_expr_python import cel
from cel_expr_python.ext import ext_bindings
from cel_expr_python.ext import ext_encoders
from cel_expr_python.ext import ext_lists
from cel_expr_python.ext import ext_math
from cel_expr_python.ext import ext_optional
from cel_expr_python.ext import ext_proto
from cel_expr_python.ext import ext_strings

_MODULES = (
    cel,
    ext_bindings,
    ext_encoders,
    ext_lists,
    ext_math,
    ext_optional,
    ext_proto,
    ext_strings,
)


@absltest.skipUnless(sys.platform == "darwin", "macOS-specific linkage")
class ExportedSymbolsTest(parameterized.TestCase):

  @parameterized.parameters(*_MODULES)
  def test_exports_only_init_function(self, module: types.ModuleType):
    output = subprocess.run(
        ["nm", "-gU", module.__file__],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    exported = {line.split()[-1] for line in output.splitlines() if line}
    init_symbol = "_PyInit_" + module.__name__.rpartition(".")[2]
    self.assertEqual(exported, {init_symbol})


if __name__ == "__main__":
  absltest.main()
