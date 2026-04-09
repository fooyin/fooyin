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

## A customisable music player

fooyin is a customisable desktop music player. It combines flexible playback, library management, playlists, and scripting tools in an interface that can be rearranged from a blank canvas or adapted from preset layouts.

The player is built around extensibility. fooyin supports plugins for widgets, decoders, tag readers, DSPs, and integrations, and includes FooScript for advanced display formatting, queries, autoplaylists, and widget behaviour.

| ![Simple layout](https://github.com/user-attachments/assets/8e8732a7-a7e5-4542-87b4-7d0a7b5e8fa0) | ![Directory browser layout](https://github.com/user-attachments/assets/d9f4ed62-e50a-419c-aa26-8801a76a6597) |
| --- | --- |
| ![Obsidian layout](https://github.com/user-attachments/assets/7dcc92be-04ea-402f-9cbb-a92ee855b893) | ![Custom layout](https://github.com/user-attachments/assets/de5b4b3a-cd9d-4520-a975-e268e472e0f9) |

## Features

### Playback

- Support for major formats including FLAC, MP3, MP4, Vorbis, Opus, WavPack, WAV, AIFF, MKA, Musepack, and Monkey's Audio
- Native support for VGM and tracker/module formats through optional plugins
- Playback of files directly from archives
- Gapless and bit-perfect playback
- ReplayGain support (including calculation)
- Configurable fade and crossfade behaviour for pause, stop, seek, manual track changes, and automatic transitions
- DSP chains with built-in and plugin-provided DSPs
- Waveform seekbar and VU meter visualisations
- Audio output and device configuration

### Library, playlists, and metadata

- Advanced filtering and search on library and playlist data
- Standard playlists plus autoplaylists
- Playback queue
- M3U/M3U8 import and export
- Library tree and directory browser views
- Built-in tag editor and metadata management tools
- Artwork embedding, downloading, viewing, exporting, and extracting

### Widgets, scripting, and customisation

- Fully customisable interface from a blank canvas or preset layouts
- Lyrics search, editing, syncing, and display
- FooScript for advanced formatting, display logic, queries, and autoplaylists
- Rich text and script formatting support across most widgets and views

### Integrations

- MPRIS support for desktop and media key integration
- Scrobbling support for Last.fm, Libre.fm, ListenBrainz, and custom services
- Discord Rich Presence

## Platform support

fooyin is developed primarily on Linux, with build support for Linux, macOS, Windows, and FreeBSD. 

Official support for Windows and macOS is coming soon.

## Roadmap

See [ROADMAP.md](ROADMAP.md) for upcoming releases and longer-term plans.

## Building from source

See [BUILD.md](BUILD.md) for dependency lists, build steps, and installation.

## Translations

Translations are managed on [Hosted Weblate](https://hosted.weblate.org/projects/fooyin/). Contributions are very welcome.
