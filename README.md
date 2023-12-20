<p align="center">
<img src="data/icons/sc-fooyin.svg" width="15%" alt="Fooyin logo">
</p>

<p align="center" style="font-size: 18px;">
<strong>Fooyin - A customisable music player</strong>
<br />
<a href="https://github.com/ludouzi/fooyin/actions/workflows/cmake.yml"><img src="https://github.com/ludouzi/fooyin/actions/workflows/cmake.yml/badge.svg" alt="Build status"></a>
</p>

## What is Fooyin?

Fooyin is a customisable music player for linux.

Fooyin features a *layout editing mode*
in which the entire user interface can be customised,
starting from a blank state or a preset layout. A few such layouts can be seen below:

 ![1](https://github.com/ludouzi/fooyin/assets/45490980/7b22ba0c-bf83-48e3-aae0-b15bf85d7346) | ![2](https://github.com/ludouzi/fooyin/assets/45490980/fe205504-a0a2-4837-8801-2ecf4499186a) 
----------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------
 ![3](https://github.com/ludouzi/fooyin/assets/45490980/0bf52cc7-d902-41c2-a179-1e3b5b01799a) | ![4](https://github.com/ludouzi/fooyin/assets/45490980/1481c0d5-2f04-45d2-a4ec-3657aec0f27a) 

Fooyin also includes a plugin system, with support for adding new widgets, features and more.

Audio playback is handled using FFmpeg and ALSA as the primary output driver.
There is also support for PipeWire and SDL2 through additional plugins, with more planned in the future.

## Features

* [x] Fully customisable layout
* [x] Filter and search collection
* [x] Create and manage playlists
* [x] Plugin system
* [x] FFmpeg backend
* [x] Tag editing
* [ ] Visualisations
* [ ] Lyrics support
* [ ] Last.fm integration
* [ ] Discogs integration

## Building from source

In order to build Fooyin from source, a C++20-compatible compiler is needed along with Git and CMake already
installed. Fooyin is also dependent on a number of other libraries, of which the following are required:

* [Qt6](https://www.qt.io) (6.5+)
* [QCoro](https://github.com/danvratil/qcoro) (0.9.0+)
* [TagLib](https://taglib.org) (1.12+)
* [FFmpeg](https://ffmpeg.org) (6.0+)
* [ALSA](https://alsa-project.org)

The following libraries are optional, but will add extra audio outputs:

* [SDL2](https://www.libsdl.org)
* [PipeWire](https://pipewire.org)

### Command line setup

First clone the remote repository on to your machine:

```
git clone git@github.com:ludouzi/fooyin.git
```

Then enter the newly created fooyin directory and invoke CMake, providing a build directory:

```
cd fooyin
cmake -B build/ -S . 
```

Fooyin can then be built using:

```
cmake --build build/
```

## License

Fooyin is Copyright (C) 2022-2023 by Luke Taylor.

Fooyin is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fooyin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fooyin. If not, see <http://www.gnu.org/licenses/>.