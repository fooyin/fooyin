# Building Fooyin

## Requirements

To build Fooyin you will need *at least*:

- CMake (3.18+)
- a C++ compiler with C++20 and coroutine support

The following libraries are required:

* [Qt6](https://www.qt.io) (6.4+)
* [QCoro](https://github.com/danvratil/qcoro) (0.9.0+)
* [TagLib](https://taglib.org) (1.12+)
* [FFmpeg](https://ffmpeg.org) (5.1+)
* [ALSA](https://alsa-project.org)

The following libraries are optional, but will add extra audio outputs:

* [SDL2](https://www.libsdl.org)
* [PipeWire](https://pipewire.org)

Platform-specific requirements are listed below.

### Ubuntu/Debian

```
sudo apt update
sudo apt install \
    build-essential pkg-config ninja-build \
    libasound2-dev qcoro-qt6-dev libtag1-dev \
    qt6-base-dev qt6-svg-dev qt6-tools-dev \
    libavcodec-dev libavfilter-dev libavformat-dev libavutil-dev libswresample-dev
```

### Arch Linux

```
sudo pacman -Syu
sudo pacman -S --needed \
  base-devel pkgconf ninja alsa-lib \
  qt6-base qt6-svg qt6-tools \
  qcoro-qt6 taglib ffmpeg
```

## Building

Open a terminal and run the following commands.
Adapt the instructions to suit your CMake generator.

```
git clone https://github.com/ludouzi/fooyin.git

cd fooyin
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

The installation directory defaults to `/usr/local`.
This can be changed by passing the option `-DCMAKE_INSTALL_PREFIX=/install/path`.

The following options can be passed to CMake to customise the build:

* `DBUILD_SHARED_LIBS` - Build fooyin's libraries as shared (ON by default)
* `DBUILD_PLUGINS` - Build the plugins included with fooyin (ON by default)
* `DBUILD_TRANSLATIONS` - Build translation files (ON by default)
* `DBUILD_TESTING` - Build tests (OFF by default)
* `DBUILD_CCACHE` - Build using CCache if found (ON by default)
* `DBUILD_PCH` - Build with precompiled header support (OFF by default)
* `DBUILD_WERROR` - Build with -Werror (OFF by default)

### Additional notes

* To build a debug version pass `-DCMAKE_BUILD_TYPE=Debug` to CMake.
