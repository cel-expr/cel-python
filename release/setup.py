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

"""Helper for building py_cel with bazel for PyPI releases."""

import os
import shutil
import subprocess
import sys
import setuptools
import setuptools.command.build_ext


class BazelExtension(setuptools.Extension):

  def __init__(self, name, target):
    super().__init__(name, sources=[])
    self.target = target


class BazelBuild(setuptools.command.build_ext.build_ext):
  """Custom build_ext command to build extensions with bazel."""

  def run(self):
    # Ensure bazel is available
    if shutil.which('bazel') is None:
      raise RuntimeError('bazel must be installed to build the extensions.')

    for ext in self.extensions:
      self.build_extension(ext)

  def build_extension(self, ext):
    # cibuildwheel executes setup.py using the target Python version.
    # Thus, we can use the current Python version as the target version for
    # the bazel build.
    python_version = f'{sys.version_info.major}.{sys.version_info.minor}'
    print(f'Building for target Python version: {python_version}')

    module_bazel_path = os.path.join(os.path.dirname(__file__), 'MODULE.bazel')
    if os.path.exists(module_bazel_path):
      sed_command = (
          "sed -i 's/python_version\\s*=\\s*\".*\"/"
          f"python_version = \"{python_version}\"/' {module_bazel_path}"
      )
      subprocess.check_call(sed_command, shell=True)
    else:
      raise RuntimeError(
          f'MODULE.bazel not found at {module_bazel_path}'
      )

    dest_path = self.get_ext_fullpath(ext.name)
    dest_dir = os.path.dirname(dest_path)
    os.makedirs(dest_dir, exist_ok=True)

    # Build with bazel
    # Use --compilation_mode=opt for release builds
    cmd = ['bazel', 'build', ext.target, '--compilation_mode=opt']
    print(f"Building {ext.name} with bazel: {' '.join(cmd)}")
    subprocess.check_call(cmd)

    # Determine the output path of the bazel build
    # We handle targets like //:py_cel and //ext:ext_math
    rel_path = ext.target.lstrip('/').replace(':', '/')
    if rel_path.startswith('/'):
      rel_path = rel_path[1:]

    # Bazel output directory
    bazel_bin = 'bazel-bin'

    # Attempt to find the generated shared library
    # Suffixes can vary by platform
    suffixes = ['.so', '.pyd', '.dylib']
    found = None

    candidate_base = os.path.join(bazel_bin, rel_path)

    for suffix in suffixes:
      candidate = candidate_base + suffix
      if os.path.exists(candidate):
        found = candidate
        break

    if not found:
      # Fallback check for exact name match
      # (less likely for extensions but possible)
      if os.path.exists(candidate_base):
        found = candidate_base

    if not found:
      raise RuntimeError(
          f'Could not find bazel output for {ext.target} at {candidate_base}.*'
      )

    print(f'Copying {found} to {dest_path}')
    shutil.copyfile(found, dest_path)


setuptools.setup(
    name='py-cel',
    ext_modules=[
        BazelExtension('py_cel.py_cel', '//py_cel:py_cel'),
        BazelExtension('py_cel.ext.ext_bindings', '//py_cel/ext:ext_bindings'),
        BazelExtension('py_cel.ext.ext_encoders', '//py_cel/ext:ext_encoders'),
        BazelExtension('py_cel.ext.ext_math', '//py_cel/ext:ext_math'),
        BazelExtension('py_cel.ext.ext_optional', '//py_cel/ext:ext_optional'),
        BazelExtension('py_cel.ext.ext_proto', '//py_cel/ext:ext_proto'),
        BazelExtension('py_cel.ext.ext_string', '//py_cel/ext:ext_string'),
    ],
    cmdclass={'build_ext': BazelBuild},
    zip_safe=False,
)
