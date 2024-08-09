# Building Fooyin

## Requirements

To build Fooyin you will need *at least*:

- CMake (3.18+)
- a C++ compiler with C++20 support

The following libraries are required:

* [Qt6](https://www.qt.io) (6.2+)
* [TagLib](https://taglib.org) (1.11.1+)
* [FFmpeg](https://ffmpeg.org) (4.4+)
* [ICU](https://icu.unicode.org/)

At least one of the following is required for audio output:

* [ALSA](https://alsa-project.org)
* [PipeWire](https://pipewire.org)
* [SDL2](https://www.libsdl.org)

The following libraries are optional:
* [KDSingleApplication](https://github.com/KDAB/KDSingleApplication) - will use 3rd party dep if not present on system
* [libvgm](https://github.com/ValleyBell/libvgm) - will use 3rd party dep if not present on system
* [OpenMPT](https://lib.openmpt.org/libopenmpt/) - for the OpenMPT audio input plugin
* [Game Music Emu](https://github.com/libgme/game-music-emu) - for the GME audio input plugin
* [libarchive](https://www.libarchive.org/) - for the archive support plugin

Platform-specific requirements are listed below.

### Debian/Ubuntu

```
sudo apt update
sudo apt install \
    g++ git cmake pkg-config ninja-build libglu1-mesa-dev libxkbcommon-dev zlib1g-dev \
    libasound2-dev libtag1-dev libicu-dev libpipewire-0.3-dev \
    qt6-base-dev libqt6svg6-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
    libavcodec-dev libavformat-dev libavutil-dev libavdevice-dev libswresample-dev \
    libopenmpt-dev libgme-dev libarchive-dev
```

### Arch Linux

```
sudo pacman -Syu
sudo pacman -S --needed \
    gcc git cmake pkgconf ninja alsa-lib pipewire icu ffmpeg \
    qt6-base qt6-svg qt6-tools kdsingleapplication taglib \
    libopenmpt libgme libarchive
```

### Fedora

```
sudo dnf update
sudo dnf install \
    cmake ninja-build glib2-devel libxkbcommon-x11-devel libxkbcommon-devel \
    alsa-lib-devel qt6-qtbase-devel qt6-qtsvg-devel qt6-qttools-devel \
    libavcodec-free-devel libavformat-free-devel libavutil-free-devel libswresample-free-devel \
    taglib-devel kdsingleapplication-qt6-devel libicu-devel pipewire-devel \
    libopenmpt-devel game-music-emu-devel libarchive-devel
```

## Building

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
* `-DBUILD_PLUGINS` - Build the plugins included with fooyin (ON by default)
* `-DBUILD_ALSA` - Build the ALSA plugin (ON by default)
* `-DBUILD_LIBVGM` - Build the libvgm plugin (ON by default)
* `-DBUILD_TRANSLATIONS` - Build translation files (ON by default)
* `-DBUILD_CCACHE` - Build using CCache if found (ON by default)
* `-DBUILD_PCH` - Build with precompiled header support (OFF by default)
* `-DBUILD_WERROR` - Build with -Werror (OFF by default)
* `-DBUILD_ASAN` - Enable AddressSanitizer (OFF by default)
* `-DINSTALL_FHS` - Install in Linux distros /usr hierarchy (ON by default)
* `-DINSTALL_HEADERS` - Install development files (OFF by default)

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
