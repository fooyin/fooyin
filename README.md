<!-- <p align="center">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/ludouzi/fooyin/master/data/images/logo-dark.svg">
  <source media="(prefers-color-scheme: light)" srcset="https://raw.githubusercontent.com/ludouzi/fooyin/master/data/images/logo.svg">
  <img alt="Fooyin logo." align="center" width=35% src="https://raw.githubusercontent.com/ludouzi/fooyin/master/data/images/logo.svg">
</picture>
</p> -->

# Fooyin

Fooyin is a customisable music player for linux.

Fooyin features a _layout editing mode_
in which the entire user interface can be customised, 
starting from a blank state or a default layout. _FooScript_ takes this further by extending the customisation to individual widgets themselves.

<p align="center">
<img src="data/images/editing.png" width="70%" style="vertical-align:middle">
</p>

## Features

* [x] Fully customisable layout
* [x] Filter and search collection
* [x] Create and manage playlists
* [x] Plugin system
* [x] FFmpeg backend
* [ ] Tag editing
* [ ] Visualisations
* [ ] Lyrics support
* [ ] Last.fm integration
* [ ] Discogs integration

## Dependencies

### Required

* [Qt6](https://www.qt.io) (6.5+)
* [QCoro](https://github.com/danvratil/qcoro) (0.9.0+)
* [TagLib](https://taglib.org) (1.12+)
* [FFmpeg](https://ffmpeg.org) (6.0+)
* [ALSA](https://alsa-project.org)

### Optional

* [SDL2](https://www.libsdl.org)
* [PipeWire](https://pipewire.org)

## Setup

```
git clone git@github.com:ludouzi/fooyin.git
cd fooyin && mkdir build && cd build
cmake .. && make -j$(nproc)
sudo make install
```
