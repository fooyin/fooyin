# Plugin SDK

The fooyin SDK contains the public headers, generated export headers, shared libraries,
CMake package files, and plugin helpers needed to build external plugins. Qt and other
system dependencies are installed separately.

> **Note:** fooyin is still under active development. Breaking changes to the SDK and
> plugin API/ABI should be expected until the stable 1.0 release.

## Architecture and compatibility

The fooyin runtime supplies the application and shared libraries that load and run plugins.
The SDK supplies the public development contract: headers, CMake targets, and development
files, including libraries needed for linking. It is built and versioned alongside fooyin.

There are two identifiers to describe compatibility:

| Identifier                       | Meaning                                                         |
|----------------------------------|-----------------------------------------------------------------|
| Plugin IID (`FOOYIN_PLUGIN_IID`) | The fooyin plugin ABI contract, checked before loading a plugin |
| SDK version and source commit    | The development snapshot used to compile the plugin             |

Within an IID, ABI compatibility is preserved across the public C++ headers,
including plugin interfaces, public class layouts, exported functions, and types exchanged
with plugins. An unavoidable, incompatible change requires an IID bump, 
which rejects plugins built for the previous contract.

An older plugin can continue to work with a newer fooyin in the same ABI contract. A newer
plugin can require symbols absent from an older fooyin even when the IID is unchanged.
fooyin currently checks the IID and loads the plugin only if it's IID matches.

## Linux baseline

The Linux SDK is produced by the existing Ubuntu 24.04 x64 application build with Qt
6.4.2 and GCC 13. Build distributable plugin binaries in the same environment
to avoid introducing newer Qt, glibc, or libstdc++ requirements.

Install these packages in an Ubuntu 24.04 build environment:

```sh
apt-get update
apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build pkg-config ca-certificates curl xz-utils \
  qt6-base-dev libqt6svg6 libqt6sql6-sqlite \
  libavcodec60 libavfilter9 libavformat60 libavutil58 libswresample4 \
  libicu74 libtag1v5 libarchive13t64 zlib1g libqcoro6core0t64 libqcoro6network0t64
```

Install any additional development packages required by the plugin itself.

## Windows baseline

The Windows SDK is currently produced by the existing fooyin Windows x64 build,
using MSVC and Qt 6.12. The archive name includes the architecture, compiler family, 
and Qt minor version, for example `fooyin-sdk-0.14.1-windows-x64-msvc-qt6.12.zip`.

Use an MSVC toolchain compatible with the SDK compiler recorded in `sdk-manifest.json`,
the matching architecture, and an MSVC build of Qt at least as new as the SDK’s Qt
version. 

After extracting the ZIP, configure a plugin from an MSVC developer shell. For example,
with the SDK at `C:/fooyin-sdk`:

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:/fooyin-sdk `
  "-DQt6_DIR=$env:QT_ROOT_DIR/lib/cmake/Qt6"
cmake --build build --config Release
```

Windows manifests also record the vcpkg baseline used by the build, for reproducing its dependency versions.

## Download and use

SDK archives are attached to fooyin releases and the nightly pre-release. 
Prefer an explicit release when building a distributable plugin, as the nightly 
download is continuously replaced and may contain incompatible changes.

For example, after setting `FOOYIN_TAG` and `FOOYIN_SDK` to the release tag and archive filename:

```sh
curl --fail --location --output sdk.tar.xz \
  "https://github.com/fooyin/fooyin/releases/download/${FOOYIN_TAG}/${FOOYIN_SDK}"
mkdir fooyin-sdk
tar -xJf sdk.tar.xz -C fooyin-sdk --strip-components=1
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PWD/fooyin-sdk"
cmake --build build
```

Existing plugins continue to use `find_package(Fooyin REQUIRED)`,
`create_fooyin_plugin()`, and the `Fooyin::Core`, `Fooyin::Gui`, and `Fooyin::Utils`
targets.

`sdk-manifest.json` records the fooyin version and source commit, the actual plugin IID
in `plugin_iid`, Qt version, build distribution, compiler, architecture, and build
configuration.

## Package an external plugin

After creating a plugin with `create_fooyin_plugin()`, use the exported
`create_fooyin_plugin_bundle()` helper to add a `package_plugin` target:

```cmake
configure_file(3rdparty/mydecoder/LICENSE "${CMAKE_CURRENT_BINARY_DIR}/bundle-licenses/mydecoder.txt" COPYONLY)
create_fooyin_plugin_bundle(
    myplugin
    NAME fooyin-myplugin
    RUNTIME_TARGETS mydecoder
    FILES COPYING README.md
    LICENSE_FILES "${CMAKE_CURRENT_BINARY_DIR}/bundle-licenses/mydecoder.txt"
)
```

Build the archive with `cmake --build build --config Release --target package_plugin`.
The helper stages files in the build directory’s `bundle/` subdirectory and writes
`fooyin-myplugin-<platform>.fyplugin` beside it. Platform directories match the bundle layout:
`linux-x64`, `linux-arm64`, `windows-x64`, `windows-arm64`. CMake 3.19 or newer is required.

The plugin target is always included. Optional arguments select the remaining files:

- `NAME`: archive basename; defaults to the plugin target name.
- `RUNTIME_TARGETS`: built shared library targets to include beside the plugin. The
  helper adds paths for loading adjacent libraries and uses their SONAME filenames on
  Unix. Libraries must already export the symbols needed by their consumers.
- `RUNTIME_FILES`: prebuilt runtime files to include beside the plugin. Their dependency
  paths must already work when deployed with the plugin.
- `FILES`: documentation files copied to the archive root.
- `LICENSE_FILES`: files copied into `licenses/`, preserving their basenames.
- `LICENSE_DIRECTORIES`: directories whose contents are copied into `licenses/`.

On Unix, the helper links the plugin and `RUNTIME_TARGETS` with only the bundle runtime
path, which is `$ORIGIN` on Linux and FreeBSD.

File paths are relative to the source directory unless absolute, and only explicitly selected dependencies are bundled.

## Build an SDK locally

On Linux, enable development headers and shared libraries, then build `package_sdk`:

```sh
cmake -S . -B build-sdk -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_PLUGINS=OFF \
  -DBUILD_TRANSLATIONS=OFF \
  -DINSTALL_HEADERS=ON
cmake --build build-sdk --target package_sdk
```

On Windows, configure a development installation using the existing vcpkg preset,
then build the same target:

```powershell
cmake --preset release-vcpkg -DINSTALL_HEADERS=ON
cmake --build build-release-vcpkg --target package_sdk --config Release
```

Use `release-vcpkg-arm64` and its corresponding build directory for ARM64.

Archives are written to the build directory’s `sdk/` subdirectory: `.tar.xz` on Linux
and `.zip` on Windows. A locally built SDK inherits the Qt and 
system requirements of its build environment.
