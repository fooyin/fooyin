# <p><img align="center" width="6%" src="data/images/fooyin.png"> Fooyin</p>


Fooyin is a customisable music player for linux. It has not yet reached a stable release so bugs and breaking changes should be expected.

Fooyin features a *layout editing mode* in which the entire user interface can be customised, starting from a blank state or a default layout. 
The engine backend is currently based on mpv, though this will change to ffmpeg at a later date, allowing for a wide range of visualisations to be built.

<p align="center">
<img src="data/images/editing.png" width="70%" style="vertical-align:middle">
</p>

## Features
-  [x] Fully customisable layout
-  [x] Filter and search collection
-  [x] Plugin system
-  [ ] Full playlist support
-  [ ] Tag editing
-  [ ] FFmpeg backend
-  [ ] Visualisations
-  [ ] Lyrics support - embedded & lrc + enhanced lrc
-  [ ] Last.fm integration
-  [ ] Discogs integration
-  [ ] Lyric querying

## Dependencies
Fooyin requires:
*  Qt6
*  Taglib (1.12)
*  Libmpv (mpv)

## Setup
```
git clone git@github.com:ludouzi/fooyin.git
cd fooyin && mkdir build && cd build
cmake .. && make -j4
sudo make install
```
