# Building Fooyin

## Requirements

To build Fooyin you will need *at least*:

- CMake (3.18+)
- a C++ compiler with C++20 and coroutine support

The following libraries are required:

* [Qt6](https://www.qt.io) (6.2+)
* [QCoro](https://github.com/danvratil/qcoro) (0.9.0+)
* [TagLib](https://taglib.org) (1.11.1+)
* [FFmpeg](https://ffmpeg.org) (4.4+)
* [ALSA](https://alsa-project.org)

The following libraries are optional, but will add extra audio outputs:

* [SDL2](https://www.libsdl.org)
* [PipeWire](https://pipewire.org)

Platform-specific requirements are listed below.

### Debian/Ubuntu

```
sudo apt update
sudo apt install \
    g++ git cmake pkg-config ninja-build libglu1-mesa-dev \
    libasound2-dev libtag1-dev \
    qt6-base-dev libqt6svg6-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
    libavcodec-dev libavformat-dev libavutil-dev \
```

### Arch Linux

```
sudo pacman -Syu
sudo pacman -S --needed \
    gcc git cmake pkgconf ninja alsa-lib \
    qt6-base qt6-svg qt6-tools \
    kdsingleapplication qcoro-qt6 taglib ffmpeg
```

## Building

1. Using a terminal, switch to the directory where fooyin will be checked out
2. Clone the fooyin repository: `git clone https://github.com/ludouzi/fooyin.git`
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
* `-DBUILD_TRANSLATIONS` - Build translation files (ON by default)
* `-DBUILD_CCACHE` - Build using CCache if found (ON by default)
* `-DBUILD_PCH` - Build with precompiled header support (OFF by default)
* `-DBUILD_WERROR` - Build with -Werror (OFF by default)
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

### Notes for package maintainers

* The install script above will handle installation of all files needed by fooyin following the typical Linux FHS.
For non-standard installations outside of this hierarchy, turn off `INSTALL_FHS`.
