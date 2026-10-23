$ErrorActionPreference = 'Stop'

$archives = @(Get-ChildItem "build-$env:CMAKE_PRESET/sdk/*.zip")
if ($archives.Count -ne 1) {
    throw 'Expected one Windows SDK archive'
}

Expand-Archive -LiteralPath $archives[0].FullName -DestinationPath 'sdk-check/relocated'
$prefixes = @(Get-ChildItem 'sdk-check/relocated' -Directory)
if ($prefixes.Count -ne 1) {
    throw 'Expected one SDK installation directory'
}

$sdkPrefix = $prefixes[0].FullName
foreach ($library in 'core', 'gui', 'utils') {
    foreach ($extension in 'lib', 'dll') {
        if (-not (Test-Path "$sdkPrefix/fooyin_$library.$extension")) {
            throw "SDK is missing fooyin_$library.$extension"
        }
    }
}

New-Item -ItemType Directory -Force 'sdk-check/plugin' | Out-Null
Copy-Item -Path @(
    'tests/core/plugins/CMakeLists.txt',
    'tests/core/plugins/testplugin.cpp',
    'tests/core/plugins/testplugin.json.in'
) -Destination 'sdk-check/plugin/'

cmake -S sdk-check/plugin -B sdk-check/build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$sdkPrefix" `
    "-DQt6_DIR=$env:QT_ROOT_DIR/lib/cmake/Qt6"

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

cmake --build sdk-check/build --config Release

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

cmake --install sdk-check/build --prefix "$sdkPrefix" --config Release

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$env:PATH = "$sdkPrefix;$env:QT_ROOT_DIR/bin;$env:VCPKG_INSTALLED_DIR/$env:VCPKG_TARGET_TRIPLET/bin;$env:PATH"
$pluginPath = Join-Path $sdkPrefix 'plugins/fyplugin_testbundleplugin.dll'
$pluginHandle = [System.Runtime.InteropServices.NativeLibrary]::Load($pluginPath)
try {
    [void][System.Runtime.InteropServices.NativeLibrary]::GetExport($pluginHandle, 'qt_plugin_instance')
} finally {
    [System.Runtime.InteropServices.NativeLibrary]::Free($pluginHandle)
}
