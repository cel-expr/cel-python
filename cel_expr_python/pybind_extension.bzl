# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""pybind_extension wrapper that keeps statically linked dependencies private."""

load("@pybind11_bazel//:build_defs.bzl", _pybind_extension = "pybind_extension")

def pybind_extension(name, linkopts = [], **kwargs):
    """Builds a pybind11 extension module that exports only its init function.

    Each extension statically links its own copy of protobuf, abseil and
    cel-cpp. On macOS, dyld coalesces exported weak definitions across every
    loaded image, so exporting those copies lets them bind to another
    extension's protobuf or abseil (e.g. ortools') and crash at import time.
    Linux is unaffected because pybind_extension links with -Bsymbolic there.

    Args:
      name: The name of the extension module.
      linkopts: Additional linker options.
      **kwargs: Passed through to pybind11_bazel's pybind_extension.
    """
    _pybind_extension(
        name = name,
        linkopts = linkopts + select({
            "@platforms//os:osx": ["-Wl,-exported_symbol,_PyInit_" + name],
            "//conditions:default": [],
        }),
        **kwargs
    )
