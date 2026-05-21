@echo off
:: Copyright 2026 Google LLC
::
:: Licensed under the Apache License, Version 2.0 (the "License");
:: you may not use this file except in compliance with the License.
:: You may obtain a copy of the License at
::
::     http://www.apache.org/licenses/LICENSE-2.0
::
:: Unless required by applicable law or agreed to in writing, software
:: distributed under the License is distributed on an "AS IS" BASIS,
:: WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
:: See the License for the specific language governing permissions and
:: limitations under the License.
::
setlocal enabledelayedexpansion
:: presubmit_windows.bat
:: Kokoro entrypoint for Windows Presubmit builds.

echo === Launching Windows Build Workflow ===
call "%~dp0build_windows.bat"
if !ERRORLEVEL! NEQ 0 (
    echo Windows Presubmit Build FAILED!
    exit /b 1
)

echo --- Bazel Test ---
bazel %STARTUP_FLAGS% test %LINK_FLAGS% --test_output=errors //...
if !ERRORLEVEL! NEQ 0 (
    echo Tests failed!
    exit /b 1
)

echo Windows Presubmit Build and Tests PASSED!
