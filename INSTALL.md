# Installing Fooyin

## Requirements

To build Fooyin you will need *at least*:

- CMake (3.16+)
- a C++ compiler with C++20 and coroutine support

The following libraries are required:

* [Qt6](https://www.qt.io) (6.5+)
* [QCoro](https://github.com/danvratil/qcoro) (0.9.0+)
* [TagLib](https://taglib.org) (1.12+)
* [FFmpeg](https://ffmpeg.org) (6.0+)
* [ALSA](https://alsa-project.org)

The following libraries are optional, but will add extra audio outputs:

* [SDL2](https://www.libsdl.org)
* [PipeWire](https://pipewire.org)

## Building

Open a terminal and run the following commands.
Adapt the instructions to suit your CMake generator.

```bash
cmake -G Ninja ../path/to/fooyin
cmake --build .
cmake --build . --target install
```

The installation directory defaults to `/usr/local`.
This can be changed by passing the option `-DCMAKE_INSTALL_PREFIX=/install/path`.

The following options can be passed to CMake to customise the build:

* `DBUILD_SHARED_LIBS` - Build fooyin's libraries as shared (ON by default).
* `DBUILD_TESTING` - Build tests (OFF by default).
* `DBUILD_PLUGINS` - Build the plugins included with fooyin (ON by default).
* `DBUILD_CCACHE` - Build using CCache if found (ON by default).
* `DBUILD_PCH` - Build with precompiled header support (OFF by default).

### Additional notes

* To build a debug version pass `-DCMAKE_BUILD_TYPE=Debug` to CMake.
* Plugins are expected to be found under `CMAKE_INSTALL_PREFIX/CMAKE_INSTALL_LIBDIR/fooyin/plugins` relative to `CMAKE_INSTALL_BINDIR`.

