# Building Fooyin

## Requirements

To build Fooyin you will need *at least*:

- CMake (3.18+)
- a C++ compiler with C++23 support

The following libraries are required:

* [Qt6](https://www.qt.io) (6.4+, 6.8+ on Windows)
* [TagLib](https://taglib.org) (1.12+)
* [FFmpeg](https://ffmpeg.org) (4.4+)
* [ICU](https://icu.unicode.org/)
* [zlib](https://zlib.net/)

At least one of the following is required for audio output:

* [ALSA](https://alsa-project.org)
* [PipeWire](https://pipewire.org)
* [PulseAudio](https://www.freedesktop.org/wiki/Software/PulseAudio/)
* [SDL2](https://www.libsdl.org)

The following libraries are optional:
* [KDSingleApplication](https://github.com/KDAB/KDSingleApplication) - will use 3rd party dep if not present on system
* [QCoro](https://github.com/qcoro/qcoro) - will use 3rd party dep if not present on system
* [libsndfile](https://libsndfile.github.io/libsndfile) - for the sndfile audio input plugin
* [OpenMPT](https://lib.openmpt.org/libopenmpt) - for the OpenMPT audio input plugin
* [Game Music Emu](https://github.com/libgme/game-music-emu) - for the GME audio input plugin
* [libarchive](https://www.libarchive.org) - for the archive support plugin
* [libebur128](https://github.com/jiixyj/libebur128) - for the ReplayGain scanner plugin
* [libcdio](https://www.gnu.org/software/libcdio/) and libcdio-paranoia - for the Audio CD plugin
* [SoundTouch](https://www.surina.net/soundtouch/) - for the SoundTouch DSP plugin
* [SoXR (libsoxr)](https://sourceforge.net/projects/soxr/) - for the SoX resampler DSP plugin
* [projectM](https://github.com/projectM-visualizer/projectm) (4.x) - for the projectM visualisation plugin

Platform-specific requirements are listed below.

### Debian/Ubuntu

```
sudo apt update
sudo apt install \
    g++ git cmake pkg-config ninja-build libglu1-mesa-dev libxkbcommon-dev zlib1g-dev \
    libasound2-dev libtag1-dev libicu-dev libpipewire-0.3-dev libpulse-dev \
    qt6-base-dev libqt6sql6-sqlite libqt6svg6-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
    libavcodec-dev libavformat-dev libavutil-dev libswresample-dev \
    libsndfile1-dev libopenmpt-dev libgme-dev libarchive-dev libebur128-dev libcdio-dev \
    libcdio-paranoia-dev libsoundtouch-dev libsoxr-dev
```

### Arch Linux

```
sudo pacman -Syu
sudo pacman -S --needed \
    gcc git cmake pkgconf ninja alsa-lib pipewire libpulse icu zlib ffmpeg \
    qt6-base qt6-svg qt6-imageformats qt6-tools kdsingleapplication \
    taglib libsndfile libopenmpt libgme libarchive libebur128 libcdio libcdio-paranoia soundtouch libsoxr
```

### Fedora

```
sudo dnf update
sudo dnf install \
    cmake ninja-build glib2-devel libxkbcommon-x11-devel libxkbcommon-devel zlib-ng-compat-devel \
    alsa-lib-devel qt6-qtbase-devel qt6-qtsvg-devel qt6-qttools-devel \
    libavcodec-free-devel libavformat-free-devel libavutil-free-devel libswresample-free-devel \
    taglib-devel kdsingleapplication-qt6-devel libicu-devel pipewire-devel pulseaudio-libs-devel \
    libsndfile-devel libopenmpt-devel game-music-emu-devel libarchive-devel libebur128-devel libcdio-devel \
    libcdio-paranoia-devel soundtouch-devel soxr-devel
```

### Windows

Official fooyin Windows builds use MSVC.

Install the following tools:

* [Visual Studio 2026](https://visualstudio.microsoft.com/vs/) with the **Desktop development with C++** workload,
  which includes vcpkg
* [Git](https://git-scm.com/download/win)
* [Qt 6](https://www.qt.io/download-qt-installer-oss) using the Qt Online Installer

Qt must be installed separately rather than through vcpkg. In the Qt installer, select an MSVC build of Qt 6.8 or
newer that matches the architecture being built.

Set the following user or system environment variable:

| Variable | Example value |
|----------|---------------|
| `QT_ROOT_DIR` | `C:\Qt\6.8.3\msvc2022_64` |

`QT_ROOT_DIR` must point to Qt's compiler-specific directory, not the top-level `C:\Qt` directory.

## Building

### Building from the command line

1. Using a terminal, switch to the directory where fooyin will be checked out
2. Clone the fooyin repository (including submodules):

```
git clone --recurse-submodules https://github.com/fooyin/fooyin.git
```

3. Switch into the directory: `cd fooyin`
4. Run CMake to generate a build environment:

```
cmake -S . -G Ninja -B <BUILD_DIRECTORY>
```

5. And then build fooyin:

```
cmake --build <BUILD_DIRECTORY>
```

* Optionally add `-j$(nproc)` to build faster

A *Release* build is built by default. This can be changed by passing either 
`Debug`, `RelWithDebInfo`, or `MinSizeRel` to `-DCMAKE_BUILD_TYPE`

The following options can be passed to CMake to customise the build:

* `-DBUILD_SHARED_LIBS` - Build fooyin's libraries as shared (ON by default)
* `-DBUILD_TESTING` - Build tests (OFF by default)
* `-DBUILD_SENSITIVE_TESTING` - Build time-sensitive tests (OFF by default; may be unreliable in heavily 
loaded environments)
* `-DBUILD_PLUGINS` - Build the plugins included with fooyin (ON by default)
* `-DPLUGIN_SELECTION` - Select bundled plugins to build. Leave empty for the default set, use `none` for no plugins, 
or use a semicolon/comma-separated list of plugin names to include or `-name` entries to exclude
* `-DBUILD_ALSA` - Build the ALSA plugin (ON by default)
* `-DBUILD_TRANSLATIONS` - Build translation files (ON by default)
* `-DBUILD_CCACHE` - Build using CCache if found (ON by default)
* `-DBUILD_PCH` - Build with precompiled header support (OFF by default)
* `-DBUILD_WERROR` - Build with -Werror (OFF by default)
* `-DBUILD_ASAN` - Enable AddressSanitizer (OFF by default)
* `-DINSTALL_FHS` - Install in Linux distros /usr hierarchy (ON by default)
* `-DINSTALL_HEADERS` - Install development files (OFF by default)

### Building with Visual Studio on Windows

Open the repository folder in Visual Studio. Visual Studio will detect `CMakePresets.json`; use the CMake preset
selector to choose one of the vcpkg presets appropriate for the desired architecture and build type:

* `debug-vcpkg` or `release-vcpkg` for x64
* `debug-vcpkg-arm64` or `release-vcpkg-arm64` for ARM64

Visual Studio uses the selected preset to configure the project and vcpkg to install the non-Qt dependencies. Once
configuration has completed, select **Build All** from the **Build** menu.

## Installing

Once built, fooyin can be installed using the following:

```
cmake --install <BUILD_DIRECTORY>
```

This will install fooyin to `/usr/local` by default.
To install to a custom location, either pass it to `-DCMAKE_INSTALL_PREFIX`, or 
use the prefix switch:

```
cmake --install <BUILD_DIRECTORY> --prefix <INSTALL_DIRECTORY>
```

## Uninstalling

To uninstall fooyin, simply pass the uninstall target to CMake like so:

```
cmake --build <BUILD_DIRECTORY> --target uninstall
```

### Notes for package maintainers

* The install script above will handle installation of all files needed by fooyin following the typical Linux FHS.
For non-standard installations outside of this hierarchy, turn off `INSTALL_FHS`.
