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

import glob
import os
import platform
import re
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
      with open(module_bazel_path, 'r') as f:
        content = f.read()
      content = re.sub(
          r'python_version\s*=\s*".*"',
          f'python_version = "{python_version}"',
          content,
      )
      with open(module_bazel_path, 'w') as f:
        f.write(content)
    else:
      raise RuntimeError(f'MODULE.bazel not found at {module_bazel_path}')

    dest_path = self.get_ext_fullpath(ext.name)
    dest_dir = os.path.dirname(dest_path)
    os.makedirs(dest_dir, exist_ok=True)

    # Build with bazel
    # Use --compilation_mode=opt for release builds
    cmd = ['bazel', 'build', ext.target, '--compilation_mode=opt']
    if sys.platform == 'win32':
      self.platform_config_windows(cmd, python_version)
    if sys.platform == 'darwin':
      self.platform_config_macos(cmd)
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

  def platform_config_windows(self, cmd, python_version):
    """Applies Windows-specific Bazel workarounds for Hermetic Python."""
    cmd.insert(1, '--output_user_root=C:/tmp')

    # 1. Get output base
    output_base = subprocess.check_output(
        ['bazel', '--output_user_root=C:/tmp', 'info', 'output_base'], text=True
    ).strip()

    # 2. Find hermetic python directory
    py_ver_underscore = python_version.replace('.', '_')
    pattern = os.path.join(
        output_base, 'external', f'*python_{py_ver_underscore}_host'
    )
    matches = glob.glob(pattern)
    if not matches:
      print(
          'Warning: Hermetic Python directory not found with pattern:'
          f' {pattern}'
      )
      return

    py_host_dir = matches[0]
    print(f'Found Hermetic Python Directory: {py_host_dir}')

    # 3. Copy libs to a space-free directory
    target_dir = r'C:\tmp\python_libs'
    os.makedirs(target_dir, exist_ok=True)

    lib_pattern = os.path.join(py_host_dir, 'libs', 'python*.lib')
    for lib_file in glob.glob(lib_pattern):
      print(f'Copying {lib_file} to {target_dir}')
      shutil.copy(lib_file, target_dir)

    dll_pattern = os.path.join(py_host_dir, 'python*.dll')
    for dll_file in glob.glob(dll_pattern):
      print(f'Copying {dll_file} to {target_dir}')
      shutil.copy(dll_file, target_dir)

    # 4. Add link flags to bazel command
    cmd.extend(['--linkopt=/LIBPATH:C:\\tmp\\python_libs', '--action_env=PATH'])

    # 5. Add to PATH in current process so bazel can find the DLLs
    os.environ['PATH'] = f"{target_dir};{os.environ.get('PATH', '')}"

  def platform_config_macos(self, cmd):
    """Applies macOS-specific Bazel configurations."""
    deployment_target = os.environ.get('MACOSX_DEPLOYMENT_TARGET', '10.13')
    cmd.extend([
        f'--macos_minimum_os={deployment_target}',
        '--cxxopt=-faligned-allocation',
    ])

    archflags = os.environ.get('ARCHFLAGS', '')
    if 'x86_64' in archflags:
      target_arch = 'x86_64'
    elif 'arm64' in archflags:
      target_arch = 'arm64'
    else:
      machine = platform.machine()
      if machine in ('AMD64', 'x86_64'):
        target_arch = 'x86_64'
      elif machine in ('arm64', 'aarch64'):
        target_arch = 'arm64'
      else:
        target_arch = machine

    print(f'Target architecture for macOS: {target_arch}', flush=True)
    cmd.append(f'--macos_cpus={target_arch}')
    cmd.append(f'--cpu=darwin_{target_arch}')
    if target_arch == 'x86_64':
      cmd.append('--platforms=//cel_expr_python:macos_x86_64')


setuptools.setup(
    name='cel-expr-python',
    ext_modules=[
        BazelExtension(
            'cel_expr_python.cel',
            '//cel_expr_python:cel',
        ),
        BazelExtension(
            'cel_expr_python.ext.ext_bindings',
            '//cel_expr_python/ext:ext_bindings',
        ),
        BazelExtension(
            'cel_expr_python.ext.ext_encoders',
            '//cel_expr_python/ext:ext_encoders',
        ),
        BazelExtension(
            'cel_expr_python.ext.ext_math',
            '//cel_expr_python/ext:ext_math',
        ),
        BazelExtension(
            'cel_expr_python.ext.ext_optional',
            '//cel_expr_python/ext:ext_optional',
        ),
        BazelExtension(
            'cel_expr_python.ext.ext_proto',
            '//cel_expr_python/ext:ext_proto',
        ),
        BazelExtension(
            'cel_expr_python.ext.ext_strings',
            '//cel_expr_python/ext:ext_strings',
        ),
    ],
    cmdclass={'build_ext': BazelBuild},
    zip_safe=False,
)
