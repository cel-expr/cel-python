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

set "IN_PRESUBMIT=1"
set "PRESUBMIT_STATUS=0"
if "%PYTHON_VERSIONS%" == "" (
    set "PYTHON_VERSIONS=3.11"
)

echo === Launching Windows Build Workflow ===
for %%V in (%PYTHON_VERSIONS%) do (
    echo --- Building Python %%V ---
    call "%~dp0build_windows.bat" %%V
    if !ERRORLEVEL! NEQ 0 (
        echo Windows Presubmit Build FAILED for Python %%V!
        set "PRESUBMIT_STATUS=1"
        goto cleanup
    )

    echo --- Bazel Test Python %%V ---
    bazel %STARTUP_FLAGS% test %LINK_FLAGS% --test_output=errors //...
    if !ERRORLEVEL! NEQ 0 (
        echo Tests failed for Python %%V!
        set "PRESUBMIT_STATUS=1"
        goto cleanup
    )

    if exist MODULE.bazel.bak (
        echo --- Restoring MODULE.bazel ---
        move /y MODULE.bazel.bak MODULE.bazel >nul
    )
)

echo Windows Presubmit Build and Tests PASSED!

:cleanup
if exist MODULE.bazel.bak (
    echo --- Restoring MODULE.bazel ---
    move /y MODULE.bazel.bak MODULE.bazel >nul
)
if "%PRESUBMIT_STATUS%" NEQ "0" (
    exit /b %PRESUBMIT_STATUS%
)
