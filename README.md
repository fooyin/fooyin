<p align="center">
<img src="data/icons/sc-fooyin.svg" width="20%" alt="fooyin logo">
</p>

<p align="center" style="font-size: 18px;">
<strong>fooyin - A customisable music player</strong>
<br />
<a href="https://github.com/ludouzi/fooyin/actions/workflows/build.yml"><img src="https://github.com/ludouzi/fooyin/actions/workflows/build.yml/badge.svg" alt="Build status"></a>
<a href="https://app.codacy.com/gh/ludouzi/fooyin/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade"><img src="https://app.codacy.com/project/badge/Grade/ae0c3e9825d849b0b64697e59e4dfea6" alt="Code quality"></a>
</p>

## What is fooyin?

fooyin is a music player built around customisation. It offers a growing list of widgets to manage and play your local music
collection. It's extendable through the use of plugins and scriptable using *FooScript*.

Audio playback is handled using FFmpeg and ALSA as the primary output driver.
There is also support for PipeWire and SDL2 through additional plugins, with more planned in the future.

A *layout editing mode* enables the entire user interface to be customised,
starting from a blank slate or a preset layout.

 ![1](https://github.com/ludouzi/fooyin/assets/45490980/94d610d8-4878-4c7a-8607-d2dd7936a8a1) | ![2](https://github.com/ludouzi/fooyin/assets/45490980/d82f619c-f43f-40d6-b9dc-54cdf88e3e11) 
----------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------
 ![3](https://github.com/ludouzi/fooyin/assets/45490980/f95f4b76-113e-4f9b-9055-376e4575b033) | ![4](https://github.com/ludouzi/fooyin/assets/45490980/4774d1ef-1618-4a02-8e26-2ef24cb2d039) 

<details>
<summary>Building a layout from scratch</summary>

https://github.com/ludouzi/fooyin/assets/45490980/e6fbce19-2c95-4a2c-b832-32b37cb41db9

</details>

Only Linux is supported at present, though other platforms will be added at a later date.

## Features

* [x] Customisable layout
* [x] Gapless playback
* [x] Filter library on any field(s)
* [x] Create and manage playlists
* [x] Extendable through plugins
* [x] Tag editing

## Building from source

See [BUILD.md](BUILD.md) for details.

## Roadmap

See [ROADMAP.md](ROADMAP.md) to learn about fooyin's planned features.
