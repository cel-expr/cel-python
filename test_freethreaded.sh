#!/bin/bash
#
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

# Runs CEL Python tests in free-threaded mode (PEP 703 / Python 3.13t).
# Supports both Blaze (Google internal) and Bazel (open-source) toolchains.

set -euo pipefail

# Detect whether we're using Blaze or Bazel.
if [ -n "${BLAZE_BIN:-}" ]; then
  BUILD_TOOL="${BLAZE_BIN}"
  IS_BLAZE=true
elif [ -n "${BAZEL_BIN:-}" ]; then
  BUILD_TOOL="${BAZEL_BIN}"
  IS_BLAZE=false
elif command -v blaze >/dev/null 2>&1 && [ -d "third_party/cel/python" ]; then
  BUILD_TOOL="blaze"
  IS_BLAZE=true
elif command -v bazel >/dev/null 2>&1; then
  BUILD_TOOL="bazel"
  IS_BLAZE=false
elif command -v bazelisk >/dev/null 2>&1; then
  BUILD_TOOL="bazelisk"
  IS_BLAZE=false
elif command -v blaze >/dev/null 2>&1; then
  BUILD_TOOL="blaze"
  IS_BLAZE=true
else
  echo "Error: Neither blaze nor bazel found in PATH." >&2
  exit 1
fi

if [ "${IS_BLAZE}" = true ]; then
  FREETHREADED_FLAG="--//third_party/bazel_rules/rules_python/python/config_settings:py_freethreaded=yes"
  DEFAULT_TARGETS=("//third_party/cel/python/...")
else
  FREETHREADED_FLAG="--@rules_python//python/config_settings:py_freethreaded=yes"
  DEFAULT_TARGETS=("//...")
fi

if [ "$#" -eq 0 ]; then
  exec "${BUILD_TOOL}" test \
    "${FREETHREADED_FLAG}" \
    --test_env=PYTHON_GIL=0 \
    "${DEFAULT_TARGETS[@]}"
else
  exec "${BUILD_TOOL}" test \
    "${FREETHREADED_FLAG}" \
    --test_env=PYTHON_GIL=0 \
    "$@"
fi
