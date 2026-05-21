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
:: build_windows.bat
:: Core Bazel Build script for CEL Python on Windows.

echo === Loading Environment Configuration ===
call "%~dp0set_env_windows.bat"
if !ERRORLEVEL! NEQ 0 (
    echo Failed to configure build environment!
    exit /b 1
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
            timeout /t %FETCH_RETRY_DELAY_S% /nobreak >nul
            goto fetch_loop
        )
    )
    echo Fetch failed permanently or max attempts reached.
    if exist fetch.log del fetch.log
    exit /b 1
)
if exist fetch.log del fetch.log

echo --- Getting Output Base ---
for /f "tokens=*" %%i in ('bazel --output_user_root=C:/tmp info output_base') do set "OUTPUT_BASE=%%i"
set "OUTPUT_BASE=!OUTPUT_BASE:/=\!"
echo Output Base: !OUTPUT_BASE!

echo --- Resolving Hermetic Python Toolchain ---
for /f "tokens=*" %%A in ('dir /b /ad "!OUTPUT_BASE!\external\*python_3_11_host" 2^>nul') do set "PY_HOST_DIR=%%A"
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

echo --- Applying VERSION Collision Fix ---
set "ANTLR_DIR=!OUTPUT_BASE!\external\antlr4-cpp-runtime+"
if exist "!ANTLR_DIR!\VERSION" (
    if not exist "!ANTLR_DIR!\VERSION.txt" (
        echo Renaming !ANTLR_DIR!\VERSION to VERSION.txt
        ren "!ANTLR_DIR!\VERSION" VERSION.txt
    )
)
if exist "!ANTLR_DIR!\version" (
    if not exist "!ANTLR_DIR!\version.txt" (
        echo Renaming !ANTLR_DIR!\version to version.txt
        ren "!ANTLR_DIR!\version" version.txt
    )
)

echo --- Bazel Build ---
bazel %STARTUP_FLAGS% build %LINK_FLAGS% //...
if !ERRORLEVEL! NEQ 0 (
    echo Build failed!
    exit /b 1
)

echo --- Build Success ---
