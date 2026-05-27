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

    echo === Loading Environment Configuration ===
    call "%~dp0set_env_windows.bat" %%V
    if !ERRORLEVEL! NEQ 0 (
        echo Failed to configure build environment!
        set "PRESUBMIT_STATUS=1"
        goto cleanup
    )

    set "BUILD_STATUS=0"

    echo --- Backing up MODULE.bazel ---
    copy MODULE.bazel MODULE.bazel.bak >nul

    echo --- Dynamically Adjusting Python Version in MODULE.bazel ---
    !PYTHON_EXE! -c "import sys; path='MODULE.bazel'; content=open(path).read(); open(path,'w').write(content.replace('python_version = \"3.11\"', 'python_version = \"!PYTHON_VERSION!\"'))"
    if !ERRORLEVEL! NEQ 0 (
        echo Failed to modify MODULE.bazel!
        set "PRESUBMIT_STATUS=1"
        goto cleanup
    )

    :: Fetch dependencies. We perform multiple attempts to absorb transient flaky network connections.
    echo --- Fetching Dependencies ---
    set ATTEMPTS=0
    :fetch_loop
    set /a ATTEMPTS+=1
    echo Fetch attempt !ATTEMPTS! of %FETCH_RETRIES%...
    bazel %STARTUP_FLAGS% fetch //... > fetch.log 2>&1
    set FETCH_STATUS=!ERRORLEVEL!
    type fetch.log
    if !FETCH_STATUS! NEQ 0 (
        findstr /i "timeout timed" fetch.log >nul
        if !ERRORLEVEL! EQU 0 (
            if !ATTEMPTS! LSS %FETCH_RETRIES% (
                echo Fetch failed with timeout. Retrying in %FETCH_RETRY_DELAY_S% seconds...
                :: Use ping instead of timeout because timeout command fails in non-interactive Kokoro environments
                :: with "ERROR: Input redirection is not supported, exiting the process immediately."
                set /a PINGS=%FETCH_RETRY_DELAY_S%+1
                ping -n !PINGS! 127.0.0.1 >nul
                goto fetch_loop
            )
        )
        echo Fetch failed permanently or max attempts reached.
        set "PRESUBMIT_STATUS=1"
        goto cleanup
    )
    if exist fetch.log del fetch.log

    echo --- Getting Output Base ---
    for /f "tokens=*" %%i in ('bazel %STARTUP_FLAGS% info output_base') do set "OUTPUT_BASE=%%i"
    set "OUTPUT_BASE=!OUTPUT_BASE:/=\!"
    echo Output Base: !OUTPUT_BASE!

    echo --- Resolving Hermetic Python Toolchain ---
    for /f "tokens=*" %%A in ('dir /b /ad "!OUTPUT_BASE!\external\*python_!PY_VER_UNDERSCORE!_host" 2^>nul') do set "PY_HOST_DIR=%%A"
    echo Hermetic Python Directory: !PY_HOST_DIR!

    if not "!PY_HOST_DIR!" == "" (
        echo --- Copying Hermetic Python import library to space-free directory ---
        if not exist C:\tmp\python_libs mkdir C:\tmp\python_libs
        copy "!OUTPUT_BASE!\external\!PY_HOST_DIR!\libs\python*.lib" C:\tmp\python_libs\
        echo --- Copying Hermetic Python DLL to space-free directory ---
        copy "!OUTPUT_BASE!\external\!PY_HOST_DIR!\python*.dll" C:\tmp\python_libs\
        set "LINK_FLAGS=--linkopt=/LIBPATH:C:\tmp\python_libs --action_env=PATH"
        set "PATH=C:\tmp\python_libs;!PATH!"
    ) else (
        echo Warning: Hermetic Python directory not found! Skipping import library copy.
    )

    echo --- Bazel Build ---
    bazel %STARTUP_FLAGS% build %LINK_FLAGS% //...
    if !ERRORLEVEL! NEQ 0 (
        echo Build failed!
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
if exist fetch.log del fetch.log
if exist MODULE.bazel.bak (
    echo --- Restoring MODULE.bazel ---
    move /y MODULE.bazel.bak MODULE.bazel >nul
)
if "%PRESUBMIT_STATUS%" NEQ "0" (
    exit /b %PRESUBMIT_STATUS%
)
