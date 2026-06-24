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
set "RELEASE_STATUS=0"
set "FETCH_RETRIES=10"
set "FETCH_RETRY_DELAY_S=10"
echo --- Installing Python 3.11 via Chocolatey ---
choco install python311 -y --no-progress
if !ERRORLEVEL! NEQ 0 (
    echo WARNING: Failed to install Python 3.11 via Chocolatey.
)

echo === Loading Environment Configuration ===
call "%~dp0set_env_windows.bat"
if !ERRORLEVEL! NEQ 0 (
    echo Failed to configure build environment!
    exit /b 1
)
:: If running locally (not on Kokoro), authenticate with gcloud.
if "%KOKORO_BUILD_ID%" == "" (
    gcloud auth application-default print-access-token --quiet >nul 2>&1
    if !ERRORLEVEL! NEQ 0 (
        gcloud auth application-default login
    )
)

echo --- Installing Release Dependencies ---
!PYTHON_EXE! -m pip install -U keyring keyrings.google-artifactregistry-auth twine cibuildwheel
if !ERRORLEVEL! NEQ 0 (
    echo Failed to install dependencies!
    exit /b 1
)

:: Use standard Windows TEMP directory for temporary build folders to avoid nested copy recursion.
set "REPO_DIR=%TEMP%\cel-python-repo-%RANDOM%"
set "TMP_DIR=%TEMP%\cel-python-build-%RANDOM%"
echo Created temporary directories: %REPO_DIR%, %TMP_DIR%

mkdir "%TMP_DIR%"

echo --- Resolving Repository Source ---
if "%DRY_RUN%" == "true" (
    echo [DRY RUN] Using local Kokoro clone instead of cloning main.
    set "SRC_DIR=%~dp0..\.."
    pushd "!SRC_DIR!"
    for /f "tokens=*" %%i in ('git tag --sort=-v:refname 2^>nul') do (
        set "VERSION=%%i"
        goto :got_local_tag
    )
    set "VERSION=0.1.2"
    :got_local_tag
    popd
) else (
    mkdir "%REPO_DIR%"
    pushd "%REPO_DIR%"
    git clone https://github.com/cel-expr/cel-python.git
    if !ERRORLEVEL! NEQ 0 (
        echo Failed to clone repository!
        set "RELEASE_STATUS=1"
        popd
        goto cleanup
    )
    cd cel-python
    for /f "tokens=*" %%i in ('git tag --sort=-v:refname') do (
        set "VERSION=%%i"
        goto :got_tag
    )
    :got_tag
    if "%VERSION%" == "" (
        echo Failed to get version tag!
        set "RELEASE_STATUS=1"
        popd
        goto cleanup
    )
    set "SRC_DIR=%REPO_DIR%\cel-python"
    popd
)

if "%VERSION:~0,1%" == "v" (
    set "VERSION=%VERSION:~1%"
)
echo Building release for version: %VERSION%

echo --- Preparing Release in Temp Directory ---
pushd "%TMP_DIR%"

xcopy /E /I /Y "%SRC_DIR%\*.*" .
if !ERRORLEVEL! NEQ 0 (
    echo Failed to copy repo contents!
    set "RELEASE_STATUS=1"
    popd
    goto cleanup
)

xcopy /Y "%SRC_DIR%\release\*.*" .
if !ERRORLEVEL! NEQ 0 (
    echo Failed to copy release configs!
    set "RELEASE_STATUS=1"
    popd
    goto cleanup
)

if exist "cel_expr_python\*_test.py" (
    del /Q "cel_expr_python\*_test.py"
)

:: Substitute $VERSION in pyproject.toml with the value of VERSION.
!PYTHON_EXE! -c "import sys; content = open('pyproject.toml').read(); open('pyproject.toml', 'w').write(content.replace('$VERSION', sys.argv[1]))" "%VERSION%"
if !ERRORLEVEL! NEQ 0 (
    echo Failed to substitute version in pyproject.toml!
    set "RELEASE_STATUS=1"
    popd
    goto cleanup
)

echo --- Pre-fetching Dependencies ---
set ATTEMPTS=0
:fetch_loop
set /a ATTEMPTS+=1
echo Fetch attempt !ATTEMPTS! of !FETCH_RETRIES!...
bazel %STARTUP_FLAGS% fetch //... > fetch.log 2>&1
set FETCH_STATUS=!ERRORLEVEL!
type fetch.log
if !FETCH_STATUS! NEQ 0 (
    findstr /i "timeout timed" fetch.log >nul
    if !ERRORLEVEL! EQU 0 (
        if !ATTEMPTS! LSS !FETCH_RETRIES! (
            echo Fetch failed with timeout. Retrying in !FETCH_RETRY_DELAY_S! seconds...
            :: Use ping instead of timeout because timeout command fails in non-interactive Kokoro environments
            :: with "ERROR: Input redirection is not supported, exiting the process immediately."
            set /a PINGS=!FETCH_RETRY_DELAY_S!+1
            ping -n !PINGS! 127.0.0.1 >nul
            goto fetch_loop
        )
    )
    echo Pre-fetch failed permanently or max attempts reached, but continuing...
)
if exist fetch.log del fetch.log

echo --- Running cibuildwheel ---
set "CEL_BAZEL_FLAGS=--config=remote-cache-windows"
if "%CIBWHEEL_BIN%" == "" (
    set "CIBWHEEL_BIN=!PYTHON_EXE! -m cibuildwheel"
)
echo Running cibuildwheel: %CIBWHEEL_BIN%
%CIBWHEEL_BIN% --platform windows --output-dir dist
if !ERRORLEVEL! NEQ 0 (
    echo cibuildwheel failed!
    set "RELEASE_STATUS=1"
    popd
    goto cleanup
)

echo --- Uploading to OSS Exit Gate ---
if "%DRY_RUN%" == "true" (
    echo [DRY RUN] Skipping upload to PyPI exit gate.
) else (
    !PYTHON_EXE! -m twine upload --repository-url https://us-python.pkg.dev/oss-exit-gate-prod/cel-expr-python--pypi dist/*
    if !ERRORLEVEL! NEQ 0 (
        echo Twine upload failed!
        set "RELEASE_STATUS=1"
        popd
        goto cleanup
    )
)

popd
echo cel-expr-python %VERSION% built and uploaded for release by OSS Exit Gate.

:cleanup
echo Cleaning up directories...
if exist "%REPO_DIR%" rd /S /Q "%REPO_DIR%"
if exist "%TMP_DIR%" rd /S /Q "%TMP_DIR%"

if "%RELEASE_STATUS%" NEQ "0" (
    exit /b %RELEASE_STATUS%
)
