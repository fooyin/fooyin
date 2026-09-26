$ErrorActionPreference = "Stop"

$CMAKE_PRESET = $env:CMAKE_PRESET
$BUILD_DIR = "build-$CMAKE_PRESET"

Write-Host "Configuring CMake with preset $CMAKE_PRESET..."
$BUILD_CCACHE = if ($env:BUILD_CCACHE) { $env:BUILD_CCACHE } else { "OFF" }
$BUILD_PCH = if ($env:BUILD_PCH) { $env:BUILD_PCH } else { "ON" }
$BUILD_TESTS = if ($env:BUILD_TESTS) { $env:BUILD_TESTS } else { "OFF" }
$cmakeArgs = @("--preset", $CMAKE_PRESET, "-DBUILD_TESTING=$BUILD_TESTS", "-DFETCH_PROJECTM=ON", "-DENABLE_SYSTEM_GLM=ON", "-DBUILD_CCACHE=$BUILD_CCACHE", "-DBUILD_PCH=$BUILD_PCH")

if ($env:VCPKG_INSTALLED_DIR) {
    $cmakeArgs += "-DVCPKG_INSTALLED_DIR=$env:VCPKG_INSTALLED_DIR"
}

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

if ($BUILD_TESTS -eq "ON") {
    Write-Host "Running tests in $BUILD_DIR..."
    & ctest --test-dir "$BUILD_DIR/tests" --build-config Release --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Tests failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }
}
