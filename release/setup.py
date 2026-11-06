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

"""Setuptools build configuration for the cel-expr-python PyPI wheel."""

import bazel_build
import setuptools


setuptools.setup(
    name='cel-expr-python',
    ext_modules=[
        bazel_build.BazelExtension(
            'cel_expr_python.cel',
            '//cel_expr_python:cel',
        ),
        bazel_build.BazelExtension(
            'cel_expr_python.ext.ext_bindings',
            '//cel_expr_python/ext:ext_bindings',
        ),
        bazel_build.BazelExtension(
            'cel_expr_python.ext.ext_encoders',
            '//cel_expr_python/ext:ext_encoders',
        ),
        bazel_build.BazelExtension(
            'cel_expr_python.ext.ext_lists',
            '//cel_expr_python/ext:ext_lists',
        ),
        bazel_build.BazelExtension(
            'cel_expr_python.ext.ext_math',
            '//cel_expr_python/ext:ext_math',
        ),
        bazel_build.BazelExtension(
            'cel_expr_python.ext.ext_optional',
            '//cel_expr_python/ext:ext_optional',
        ),
        bazel_build.BazelExtension(
            'cel_expr_python.ext.ext_proto',
            '//cel_expr_python/ext:ext_proto',
        ),
        bazel_build.BazelExtension(
            'cel_expr_python.ext.ext_strings',
            '//cel_expr_python/ext:ext_strings',
        ),
    ],
    cmdclass={'build_ext': bazel_build.BazelBuild},
    zip_safe=False,
)
