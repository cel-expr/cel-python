# PowerShell script for Windows Presubmit
$ErrorActionPreference = 'Stop'

Write-Host '--- Environment Info ---'
Write-Host "Host Name: $env:COMPUTERNAME"
Write-Host "User Name: $env:USERNAME"
Write-Host "Current Directory: $(Get-Location)"

# Change directory to the repository root
Set-Location "$PSScriptRoot\..\.."
Write-Host "New Directory: $(Get-Location)"

$PYTHON_EXE = 'python'
if (Get-Command 'python3.11' -ErrorAction SilentlyContinue) {
    $PYTHON_EXE = 'python3.11'
}
else {
    if (Test-Path 'C:\Python311\python.exe') {
        $PYTHON_EXE = 'C:\Python311\python.exe'
    }
}

Write-Host '--- Python Version ---'
& $PYTHON_EXE --version

# Ensure Bazel is available
Write-Host '--- Bazel Version ---'
& bazel version

# Build all targets
Write-Host '--- Bazel Build ---'
& bazel build --config=windows //...

if ($LASTEXITCODE -ne 0) {
    Write-Host 'Build failed!'
    exit 1
}

# Run all tests
Write-Host '--- Bazel Test ---'
& bazel test --config=windows --test_output=errors //...

if ($LASTEXITCODE -ne 0) {
    Write-Host 'Tests failed!'
    exit 1
}

Write-Host '--- Success ---'
Write-Host 'Build and tests passed successfully.'
