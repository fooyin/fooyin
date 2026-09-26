$ErrorActionPreference = "Stop"

$CMAKE_PRESET = $env:CMAKE_PRESET
if (-not $CMAKE_PRESET) {
    Write-Error "CMAKE_PRESET is not set."
    exit 1
}

$BUILD_DIR = "build-$CMAKE_PRESET"
$ARTIFACTS_DIR = Join-Path $env:GITHUB_WORKSPACE "artifacts"

if (-not (Test-Path $ARTIFACTS_DIR)) {
    Write-Host "Creating artifacts directory: $ARTIFACTS_DIR"
    New-Item -ItemType Directory -Path $ARTIFACTS_DIR | Out-Null
}

Write-Host "Packaging project..."
& cmake --build $BUILD_DIR --target package --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake packaging failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}

Write-Host "Moving packages to $ARTIFACTS_DIR..."
$packageTypes = "*.zip", "*.exe"
foreach ($type in $packageTypes) {
    Get-ChildItem -Path $BUILD_DIR -Filter $type -File | Move-Item -Destination $ARTIFACTS_DIR -Force
}

Write-Host "All packages have been moved to $ARTIFACTS_DIR"
