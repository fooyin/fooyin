# Building Fooyin

## Requirements

To build Fooyin you will need *at least*:

- CMake (3.18+)
- a C++ compiler with C++23 support

The following package managers are required:
* [brew](https://brew.sh) (Only if building on MacOS)

The following libraries are required:

* [Qt6](https://www.qt.io) (6.4+)
* [TagLib](https://taglib.org) (1.12+)
* [FFmpeg](https://ffmpeg.org) (4.4+)
* [ICU](https://icu.unicode.org/)

At least one of the following is required for audio output:

* [ALSA](https://alsa-project.org)
* [PipeWire](https://pipewire.org)
* [SDL2](https://www.libsdl.org) (Needed for MacOS)

The following libraries are optional:
* [KDSingleApplication](https://github.com/KDAB/KDSingleApplication) - will use 3rd party dep if not present on system
* [QCoro](https://github.com/qcoro/qcoro) - will use 3rd party dep if not present on system
* [libsndfile](https://libsndfile.github.io/libsndfile) - for the sndfile audio input plugin
* [OpenMPT](https://lib.openmpt.org/libopenmpt) - for the OpenMPT audio input plugin
* [Game Music Emu](https://github.com/libgme/game-music-emu) - for the GME audio input plugin
* [libarchive](https://www.libarchive.org) - for the archive support plugin
* [libebur128](https://github.com/jiixyj/libebur128) - for the ReplayGain scanner plugin

Platform-specific requirements are listed below.

### Debian/Ubuntu

```
sudo apt update
sudo apt install \
    g++ git cmake pkg-config ninja-build libglu1-mesa-dev libxkbcommon-dev zlib1g-dev \
    libasound2-dev libtag1-dev libicu-dev libpipewire-0.3-dev \
    qt6-base-dev libqt6svg6-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
    libavcodec-dev libavformat-dev libavutil-dev libavdevice-dev libswresample-dev \
    libsndfile1-dev libopenmpt-dev libgme-dev libarchive-dev libebur128-dev
```

### Arch Linux

```
sudo pacman -Syu
sudo pacman -S --needed \
    gcc git cmake pkgconf ninja alsa-lib pipewire icu ffmpeg \
    qt6-base qt6-svg qt6-imageformats qt6-tools kdsingleapplication \
    taglib libsndfile libopenmpt libgme libarchive libebur128
```

### Fedora

```
sudo dnf update
sudo dnf install \
    cmake ninja-build glib2-devel libxkbcommon-x11-devel libxkbcommon-devel \
    alsa-lib-devel qt6-qtbase-devel qt6-qtsvg-devel qt6-qttools-devel \
    libavcodec-free-devel libavformat-free-devel libavutil-free-devel libswresample-free-devel \
    taglib-devel kdsingleapplication-qt6-devel libicu-devel pipewire-devel \
    libsndfile-devel libopenmpt-devel game-music-emu-devel libarchive-devel libebur128-devel
```

### MacOS

```
brew update && brew upgrade
brew install git cmake ninja qt taglib ffmpeg icu4c@76 sdl2
```

## Building

1. Using a terminal, switch to the directory where fooyin will be checked out
2. Clone the fooyin repository (including submodules):

```
git clone https://github.com/fooyin/fooyin.git
```

3. Switch into the directory: `cd fooyin`
4. Run CMake to generate a build environment (Go to step 4.5 if on MacOS, if not skip step 4.5):

```
cmake -S . -G Ninja -B <BUILD_DIRECTORY>
```

4.5. MacOS only (Skip if not on MacOS)
you have to generate the build environment using this command instead

```
cmake -S . -G Ninja -B <BUILD_DIRECTORY> -DCMAKE_PREFIX_PATH=/usr/local/opt/icu4c
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
* `-DBUILD_TRANSLATIONS` - Build translation files (ON by default)
* `-DBUILD_CCACHE` - Build using CCache if found (ON by default)
* `-DBUILD_PCH` - Build with precompiled header support (OFF by default)
* `-DBUILD_WERROR` - Build with -Werror (OFF by default)
* `-DBUILD_ASAN` - Enable AddressSanitizer (OFF by default)
* `-DINSTALL_FHS` - Install in Linux distros /usr hierarchy (ON by default)
* `-DINSTALL_HEADERS` - Install development files (OFF by default)

## Installing (Scroll down to next section if on MacOS)

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

## Installing on MacOS
1. Open up finder and your terminal.
2. in finder find where you cloned fooyin and follow this path "<BUILD_DIRECTORY>/run/bin"
3. Now you should see an executable called fooyin.
4. Double Click fooyin to make sure you didn't build it wrong. After that works close it.
5. switch to your terminal, and cd into the same directory you got to in finder.
6. run the command ```pwd``` to get the full directory path.
7. then open up your apps, and open an app called "Automator".
8. Choose to create a new application.
9. search shell script, find run shell script, and drag it into the spot in the middle.
10. choose your interpreter as ```/usr/bash```.
11. Then copy the output of the pwd command and paste it into the text box in automator.
12. then add ```/fooyin; exit;``` to the end of it.
13. then in the corner of automator click "file -> save".
14. Make sure to set the type to "Application" and the app name to fooyin when you're saving.
15. Find this new app and it will have the default icon.
16. Go to where you cloned fooyin and copy (CMD+C on MacOS) the "512-fooyin.png" file from your fooyin build in ```data/icons``` or download it from github.
17. Go to "Applications", find your fooyin app, right click on it, click get info, then click on the little tiny icon in the top-left corner of that menu.
18. Once clicked once press CMD+V to paste.
19. You should now have an application that has the fooyin icon and when clicked on launches fooyin!

### Why build and install process is different for MacOS
For ```cmake -S . -G Ninja -B <BUILD_DIRECTORY> -DCMAKE_PREFIX_PATH=/usr/local/opt/icu4c```:
  - The ICU Library is not findable by cmake the way brew installs it so you must tell cmake where it is manually.
  - When you run ```cmake --install <BUILD_DIRECTORY>``` it does not install behind the scences libraries such as lib-fooyingui to your "/usr/local" only the fooyin executable, which means fooyin can't open.

## Uninstalling

To uninstall fooyin, simply pass the uninstall target to CMake like so:

```
cmake --build <BUILD_DIRECTORY> --target uninstall
```

On MacOS also delete the fooyin.app in your Applications folder

### Notes for package maintainers

* The install script above will handle installation of all files needed by fooyin following the typical Unix (Linux & MacOS) FHS.
For non-standard installations outside of this hierarchy, turn off `INSTALL_FHS`.
