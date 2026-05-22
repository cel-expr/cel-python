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
:: Configures the Windows environment, proxies, startup flags, caches BCR assets, and ANTLR workarounds.

echo --- Environment Info ---
echo Host Name: %COMPUTERNAME%
echo User Name: %USERNAME%
echo Current Directory: %CD%

:: Change directory to the repository root (moving up from release/kokoro)
cd /d "%~dp0..\.."
echo New Directory: %CD%

set "no_proxy=bcr.bazel.build,bazel.build"
echo no_proxy set to %no_proxy%

echo --- Locating Bash ---
where bash.exe

set "BAZEL_SH=C:\msys64\usr\bin\bash.exe"
echo BAZEL_SH set to %BAZEL_SH%

:: Configure a very short Bazel output user root to completely bypass the Windows 260-character path length limit (MAX_PATH)
set "STARTUP_FLAGS=--output_user_root=C:/tmp"

:: Configure retry parameters for Bazel fetch to absorb transient download timeouts
set "FETCH_RETRIES=10"
set "FETCH_RETRY_DELAY_S=10"

:: Configure JVM proxy bypasses to prevent corporate proxy interception of Google API/GCS/Metadata server connections
set "STARTUP_FLAGS=%STARTUP_FLAGS% --host_jvm_args=-Dhttp.nonProxyHosts=bcr.bazel.build^|*.bazel.build^|storage.googleapis.com^|*.googleapis.com^|metadata.google.internal^|169.254.169.254"

:: Force JVM to prefer the IPv4 network stack to absorb sporadic IPv6 network drops
set "STARTUP_FLAGS=%STARTUP_FLAGS% --host_jvm_args=-Djava.net.preferIPv4Stack=true"

echo STARTUP_FLAGS set to %STARTUP_FLAGS%

echo --- Bazel Version ---
bazel %STARTUP_FLAGS% version

:: Detect Python executable first (needed for downloading BCR assets dynamically)
set PYTHON_EXE=python
where python3.11 >nul 2>&1
if !ERRORLEVEL! EQU 0 (
    set PYTHON_EXE=python3.11
) else if exist C:\Python311\python.exe (
    set PYTHON_EXE=C:\Python311\python.exe
)
echo Python set to %PYTHON_EXE%

echo --- Python Version ---
!PYTHON_EXE! --version
