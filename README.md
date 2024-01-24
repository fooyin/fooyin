<p align="center">
<img src="data/icons/sc-fooyin.svg" width="20%" alt="Fooyin logo">
</p>

<p align="center" style="font-size: 18px;">
<strong>Fooyin - A customisable music player</strong>
<br />
<a href="https://github.com/ludouzi/fooyin/actions/workflows/build.yml"><img src="https://github.com/ludouzi/fooyin/actions/workflows/build.yml/badge.svg" alt="Build status"></a>
<a href="https://app.codacy.com/gh/ludouzi/fooyin/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade"><img src="https://app.codacy.com/project/badge/Grade/ae0c3e9825d849b0b64697e59e4dfea6" alt="Code quality"></a>
</p>

## What is Fooyin?

Fooyin is a music player built around customisation. It offers a growing list of widgets to manage and play your local music
collection. It's extendable through the use of plugins and scriptable using *FooScript*.

Audio playback is handled using FFmpeg and ALSA as the primary output driver.
There is also support for PipeWire and SDL2 through additional plugins, with more planned in the future.

A *layout editing mode* enables the entire user interface to be customised,
starting from a blank slate or a preset layout. A few such layouts can be seen below:

 ![1](https://github.com/ludouzi/fooyin/assets/45490980/7b22ba0c-bf83-48e3-aae0-b15bf85d7346) | ![2](https://github.com/ludouzi/fooyin/assets/45490980/fe205504-a0a2-4837-8801-2ecf4499186a) 
----------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------
 ![3](https://github.com/ludouzi/fooyin/assets/45490980/0bf52cc7-d902-41c2-a179-1e3b5b01799a) | ![4](https://github.com/ludouzi/fooyin/assets/45490980/1481c0d5-2f04-45d2-a4ec-3657aec0f27a) 

<details>
<summary>Building a layout from scratch</summary>

https://github.com/ludouzi/fooyin/assets/45490980/e6fbce19-2c95-4a2c-b832-32b37cb41db9

</details>

Fooyin is currently only supported on linux, though other platforms will be added at a later date.

## Features

* [x] Customisable layout
* [x] Filter and search music collection
* [x] Create and manage playlists
* [x] Extendable through plugins
* [x] Tag editing
* [ ] Visualisations
* [ ] Lyrics support
* [ ] Last.fm and Discogs integration

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