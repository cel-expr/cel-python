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
:: set_env_windows.bat
:: Configures the Windows environment

echo --- Environment Info ---
echo Host Name: %COMPUTERNAME%
echo User Name: %USERNAME%
echo Current Directory: %CD%

:: Change directory to the repository root (moving up from release/kokoro)
cd /d "%~dp0..\.."
echo New Directory: %CD%

echo --- Locating Bash ---
where bash.exe

set "BAZEL_SH=C:\msys64\usr\bin\bash.exe"
echo BAZEL_SH set to %BAZEL_SH%

:: Configure a very short Bazel output user root to completely bypass the Windows 260-character path length limit (MAX_PATH)
set "STARTUP_FLAGS=--output_user_root=C:/tmp"

:: Configure retry parameters for Bazel fetch to absorb transient download timeouts
set "FETCH_RETRIES=10"
set "FETCH_RETRY_DELAY_S=10"

echo --- Bazel Version ---
bazel %STARTUP_FLAGS% version

set "PYTHON_VERSION=%~1"
if "%PYTHON_VERSION%" == "" (
    set "PYTHON_VERSION=3.11"
)
set "PY_VER_UNDERSCORE=%PYTHON_VERSION:.=_%"
set "PY_VER_NO_DOT=%PYTHON_VERSION:.=%"

echo Python Version Selected: %PYTHON_VERSION%

:: Detect Python executable first (needed for downloading BCR assets dynamically)
set PYTHON_EXE=python
where python%PYTHON_VERSION% >nul 2>&1
if !ERRORLEVEL! EQU 0 (
    set PYTHON_EXE=python%PYTHON_VERSION%
) else if exist C:\Python%PY_VER_NO_DOT%\python.exe (
    set PYTHON_EXE=C:\Python%PY_VER_NO_DOT%\python.exe
)
echo Python set to %PYTHON_EXE%
exit /b 0
