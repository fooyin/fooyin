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
starting from a blank state or a default layout.
The engine backend is currently based on mpv,
though this will change to ffmpeg at a later date,
allowing for a wide range of visualisations to be built.

<p align="center">
<img src="data/images/editing.png" width="70%" style="vertical-align:middle">
</p>

## Features

-  [x] Fully customisable layout
-  [x] Filter and search collection
-  [x] Plugin system
-  [x] Playlist support
-  [ ] Tag editing
-  [ ] FFmpeg backend
-  [ ] Visualisations
-  [ ] Lyrics support
-  [ ] Last.fm integration
-  [ ] Discogs integration

## Dependencies

### Required

-  Qt6 (6.5+)
-  QCoro (0.9.0+)
-  TagLib (1.12+)
-  FFmpeg (6.0+)
-  ALSA

### Optional

-  SDL2
-  PipeWire

## Setup

```
git clone git@github.com:ludouzi/fooyin.git
cd fooyin && mkdir build && cd build
cmake .. && make -j$(nproc)
sudo make install
```
