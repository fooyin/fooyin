<p align="center">
<img src="https://github.com/ludouzi/fooyin/assets/45490980/a6c6923a-5de3-4d29-a6e9-f73ebd5181ac" width="40%" alt="fooyin logo">
</p>

<hr />

<p align="center" style="font-size: 18px;">
<a href="https://github.com/fooyin/fooyin/actions/workflows/build.yml"><img src="https://github.com/fooyin/fooyin/actions/workflows/build.yml/badge.svg" alt="Build status"></a>
<a href="https://app.codacy.com/gh/fooyin/fooyin/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade"><img src="https://app.codacy.com/project/badge/Grade/ae0c3e9825d849b0b64697e59e4dfea6" alt="Code quality"></a>
<a href="https://hosted.weblate.org/engage/fooyin/"><img src="https://hosted.weblate.org/widget/fooyin/svg-badge.svg" alt="Translation status" /></a>
<a href="https://repology.org/project/fooyin/versions"><img src="https://repology.org/badge/tiny-repos/fooyin.svg" alt="Packaging status"></a>
</p>

<hr />

<p align="center">
<a href="https://fooyin.org">Website</a> ·
<a href="https://fooyin.org/download">Download</a> ·
<a href="https://docs.fooyin.org/en/latest/">Documentation</a> ·
<a href="https://docs.fooyin.org/en/latest/quick-start/quick-start.html">Quick start</a> ·
<a href="https://github.com/fooyin/fooyin/releases">Releases</a>
</p>

## A customisable music player

fooyin is a customisable desktop music player. It combines flexible playback, library management, playlists, and scripting tools in an interface that can be rearranged from a blank canvas or adapted from preset layouts.

It's built around extensibility and supports plugins for widgets, decoders, tag readers, DSPs, and integrations, and includes FooScript for advanced display formatting, queries, autoplaylists, and widget behaviour.

| ![Simple layout](https://fooyin.org/assets/images/simple.webp)     | ![Directory browser layout](https://fooyin.org/assets/images/browser.webp) |
|--------------------------------------------------------------------|----------------------------------------------------------------------------|
| ![Obsidian layout](https://fooyin.org/assets/images/obsidian.webp) | ![Gallery layout](https://fooyin.org/assets/images/gallery.webp)             |
| ![Radio layout](https://fooyin.org/assets/images/radio.webp)       | ![Custom layout](https://fooyin.org/assets/images/custom.webp)             |

## Features

### Playback

- Support for major audio formats and containers, including FLAC, MP3, MP4, Vorbis, Opus, WavPack, WAV, AIFF, MKA, Musepack, and Monkey's Audio
- Native support for VGM and tracker/module formats through optional plugins
- Playback of files directly from archives
- Internet radio discovery and remote audio stream playback
- Gapless and bit-perfect playback
- Configurable fade and crossfade behaviour for pause, stop, seek, manual track changes, and automatic transitions
- Waveform seekbar, spectrum, and other visualisations
- Audio output and device configuration

### Audio and metadata tools

- ReplayGain playback support and scanning
- DSP chains with built-in and plugin-provided DSPs
- Built-in tag editor and metadata management tools
- Artwork embedding, downloading, viewing, exporting, and extracting
- File renaming, copying, moving, and deletion
- Audio conversion to WAV, FLAC, ALAC, WavPack, MP3, AAC, Vorbis, and Opus with reusable presets

### Library and playlists

- Advanced filtering and search on library and playlist data
- Standard playlists plus autoplaylists
- Playback queue
- M3U/M3U8 import and export
- Library tree and directory browser views

### Widgets, scripting, and customisation

- Fully customisable interface from a blank canvas or preset layouts
- Lyrics search, editing, syncing, and display
- FooScript for advanced formatting, display logic, queries, and autoplaylists
- Rich text and script formatting support across most widgets and views

### Integrations

- MPRIS support for desktop and media key integration
- Scrobbling support for Last.fm, Libre.fm, ListenBrainz, and custom services
- Discord Rich Presence

## Installation

The [download page](https://fooyin.org/download) lists the Flathub package and packages available through Linux distribution repositories. 
Release notes and downloadable artifacts are published on [GitHub Releases](https://github.com/fooyin/fooyin/releases).

## Building from source

See [BUILD.md](BUILD.md) for dependencies, build options, and installation instructions.

## Platform support

fooyin is developed and supported on Linux and Windows. Build configurations and CI workflows are also maintained for macOS.

Official macOS support is coming soon.

## Documentation and support

- Read the [documentation](https://docs.fooyin.org/en/latest/) or follow the [quick-start guide](https://docs.fooyin.org/en/latest/quick-start/quick-start.html).
- Visit the [support page](https://fooyin.org/support) for help and frequently asked questions.
- Report bugs and request features through the [GitHub issue tracker](https://github.com/fooyin/fooyin/issues).
- See [ROADMAP.md](ROADMAP.md) for upcoming releases and long-term plans, or [CHANGELOG.md](CHANGELOG.md) for previous changes.

## Contributing and translations

Contributions through code, bug reports, documentation, testing, translation, or user support are welcome. Before contributing, please read [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md).

Translations are managed on [Hosted Weblate](https://hosted.weblate.org/projects/fooyin/). To test a translation locally, follow the instructions in [issue #679](https://github.com/fooyin/fooyin/issues/679).

## Licence

fooyin is free software released under the [GNU General Public License, version 3](COPYING).

## Donate

If you would like to support fooyin development, see the [donate page](https://fooyin.org/donate).
