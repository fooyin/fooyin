$ErrorActionPreference = "Stop"

$CMAKE_PRESET = $env:CMAKE_PRESET
$BUILD_DIR = "build-$CMAKE_PRESET"
$ARTIFACTS_DIR = Join-Path $env:GITHUB_WORKSPACE "artifacts"

if (-not (Test-Path $ARTIFACTS_DIR)) {
    Write-Host "Creating artifacts directory: $ARTIFACTS_DIR"
    New-Item -ItemType Directory -Path $ARTIFACTS_DIR | Out-Null
}

Write-Host "Configuring CMake with preset $CMAKE_PRESET..."
$BUILD_CCACHE = if ($env:BUILD_CCACHE) { $env:BUILD_CCACHE } else { "OFF" }
$BUILD_PCH = if ($env:BUILD_PCH) { $env:BUILD_PCH } else { "ON" }
$cmakeArgs = @("--preset", $CMAKE_PRESET, "-DBUILD_TESTING=OFF", "-DFETCH_PROJECTM=ON", "-DBUILD_CCACHE=$BUILD_CCACHE", "-DBUILD_PCH=$BUILD_PCH")

if ($env:QT_HOST_PATH) {
    $cmakeArgs += "-DQT_HOST_PATH=$env:QT_HOST_PATH"
    $cmakeArgs += "-DQt6LinguistTools_DIR=$env:QT_HOST_PATH/lib/cmake/Qt6LinguistTools"
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}

Write-Host "Building project in $BUILD_DIR..."
& cmake --build $BUILD_DIR --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake build failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}
if ($BUILD_CCACHE -eq "ON" -and (Get-Command ccache -ErrorAction SilentlyContinue)) {
    & ccache --show-stats
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
