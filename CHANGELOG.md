# Changelog

## [0.4.0](https://github.com/ludouzi/fooyin/releases/tag/v0.3.10) (2024-04-06)

### Features

* DB: Schema migration support ([#61](https://github.com/ludouzi/fooyin/pull/61))
* Engine: Configurable buffer size
* Playlist: Global playback queue
* Plugins: Add options to disable individual plugins
* Plugins: MPRIS support
* Plugins: Waveform seekbar ([#60](https://github.com/ludouzi/fooyin/pull/64), [#52](https://github.com/ludouzi/fooyin/issues/52))
* Scripting: Custom metadata support
* Scripting: Formatting support in playlist ([#63](https://github.com/ludouzi/fooyin/pull/63))
* SearchWidget: Add option to change placeholder text
* Seekbar: Add options to toggle labels, elapsed total
* Widgets: Directory browser ([#58](https://github.com/ludouzi/fooyin/pull/58), [#54](https://github.com/ludouzi/fooyin/issues/54))

### Changes
* Controls: Add button tooltips
* Engine: Don't start playing if seeking when paused
* Interface: Make multiple widgets responsive to DPI
* Layout editing mode: Add options to copy/paste and move widgets
* Layout editing mode: Add undo/redo support
* Playlist: Add playing column to indicate status
* Playlist: Make multi-column mode the default
* Playlist: Switch to playlist when using File->New Playlist
* Plugins: Make the ALSA output a separate plugin
* Settings: Make dialog modal (block rest of application)
* Track: Handle multiple album artists

### Fixes

* Controls: Fix player commands being issued twice
* CoverWidget: Fix no cover placeholder being shown unexpectedly
* DB: Fix out of memory errors when reading tracks
* Engine: Fix playback of some audio types
* Engine: Fix silence on next track if previously paused
* Engine: Fix silence when resuming from pause on some outputs
* Filters: Filter incoming tracks through active filters
* Filters: Fix frozen widgets when updating tracks
* Filters: Fix updated tracks being added to all filters
* Filters: Restore current selection when tracks are updated
* Library: Fix searching for album artwork in file directory ([#59](https://github.com/ludouzi/fooyin/issues/59))
* LibraryTree: Fix hang when selected 'all music' row
* LibraryTree: Restore state when updating tracks
* Playlist: Fix crash when dropping tracks into playlist
* Playlist: Fix crash when inserting same tracks multiple times
* Playlist: Fix crashes when moving tracks
* Playlist: Fix incorrect order of tracks on startup
* Playlist: Fix moving/removing tracks in multi-column mode
* Playlist: Fix partial loading of album covers
* Playlist: Insert tracks in correct order
* PlaylistOrganiser: Fix crashes when removing rows
* SearchWidget: Fix crash using search after changing connections ([#53](https://github.com/ludouzi/fooyin/issues/53))
* Seekbar: Respond to external position changes


## [0.3.10](https://github.com/ludouzi/fooyin/releases/tag/v0.3.10) (2024-02-10)

### Changes
* General build system improvements

### Fixes
* StatusWidget: Fix flickering when playing a track with an ongoing library scan ([#47](https://github.com/ludouzi/fooyin/pull/47))


## [0.3.9](https://github.com/ludouzi/fooyin/releases/tag/v0.3.9) (2024-02-10)

### Fixes

* Build system fixes

## [0.3.8](https://github.com/ludouzi/fooyin/releases/tag/v0.3.8) (2024-02-10)

### Changes

* General build system improvements
* Add support for building on Ubuntu 22.04 ([#49](https://github.com/ludouzi/fooyin/pull/49))
* Filters: Disable filter selection playlist by default
* PlaylistOrganiser: Clear selection when changing current playlist

### Fixes

* LibraryTree: Fix duplicate tracks when switching layouts ([#44](https://github.com/ludouzi/fooyin/issues/44))
* Library: Correctly sort tracks with large track numbers ([#46](https://github.com/ludouzi/fooyin/issues/46))
* PlaylistOrganiser: Resolve crash when removing all playlists ([#48](https://github.com/ludouzi/fooyin/issues/48))
* PlaylistWidget: Restore state on initialisation to keep playing icon active ([#50](https://github.com/ludouzi/fooyin/issues/50))
* PlaylistWidget: Reset state/history when using send to new/current playlist
* PlaylistTabs: Correctly restore index to current playlist on initialisation
* Translations: Return correct translation path
* Track: Fix passing track-related containers in queued signals
* ScriptSandbox: Fix setting default state
* AudioDecoder: Fix crashes when decoding files in quick succession


## [0.3.7](https://github.com/ludouzi/fooyin/releases/tag/v0.3.7) (2024-02-05)

### Features

* Add support for changing language (English and Chinese for now) ([#40](https://github.com/ludouzi/fooyin/pull/40))

### Fixes

* Fix reading audio properties using older TagLib versions ([#41](https://github.com/ludouzi/fooyin/issues/41))
* Fix removing custom tags from mp4 tags
* Fix library and track scan requests not running
* Improve ability to find installed FFmpeg libraries
* Fix high CPU usage when playing tracks (~10% -> ~1%)
* Fix playlist not switching when a playlist is removed
* Fix occasional crash when removing tracks from a playlist

## [0.3.6](https://github.com/ludouzi/fooyin/releases/tag/v0.3.6) (2024-02-02)

### Changes

* Implement AudioDecoder as a separate, self-contained object to handle all file decoding ([#39](https://github.com/ludouzi/fooyin/pull/39))
* Improve plugin naming scheme (fyplugin_name)

### Fixes

* Make creation and passing of AudioOutput to AudioEngine thread-safe
* Add additional safety checks to pipewire output


## [0.3.5](https://github.com/ludouzi/fooyin/releases/tag/v0.3.5) (2024-02-01)

### Fixes

* Fix function call in AudioRenderer


## [0.3.4](https://github.com/ludouzi/fooyin/releases/tag/v0.3.4) (2024-01-31)

### Changes

* Remove HoverMenu, LogSlider and MenuHeader from public API

### Fixes

* Fix crashes using PipeWire output

### Packaging

* Further improve plugin CMake setup
* Correctly pass vars to CMakeMacros for plugin development
* Install license, readme to data dir
* Add a CMake uninstall target
* Overhaul build instructions; see [BUILD.md](https://github.com/ludouzi/fooyin/blob/master/BUILD.md)

## [0.3.3](https://github.com/ludouzi/fooyin/releases/tag/v0.3.3) (2024-01-31)

### Changes

* Simplify plugin CMake setup

### Fixes

* Fix plugin/library rpath issues


## [0.3.2](https://github.com/ludouzi/fooyin/releases/tag/v0.3.2) (2024-01-29)

### Changes

* Move AudioEngine to public API
* Improve AudioBuffer implementation
* Support TagLib 2.0 ([#38](https://github.com/ludouzi/fooyin/issues/38))


## [0.3.1](https://github.com/ludouzi/fooyin/releases/tag/v0.3.1) (2024-01-26)

### Changes

* Rewrite library scan request handling
* Return a ScanRequest for MusicLibrary::rescan

### Fixes

* Only report library scan progress on stopping thread if we're actually running
* Remove leftover debug message


## [0.3.0](https://github.com/ludouzi/fooyin/releases/tag/v0.3.0) (2024-01-26)

### Features
* Command line support
* Support opening files/directories with fooyin

### Changes

* Create AudioBuffer, AudioFormat to handle audio data ([#37](https://github.com/ludouzi/fooyin/pull/37))
* Add widget context to TrackSelectionController
* Automatically switch to playlist when using 'Send to new playlist' option
* Allow starting playlists without explicitly setting the current track index
* Emit playlistTracksChanged in createPlaylist if playlist isn't new
* SDL2 plugin is now a push-based audio output

### Fixes

* Only add files with supported extensions
* Erase associated state and undo history when removing playlists
* Only restore previous playlist state when not resetting
* Set correct flag when appending tracks to a playlist
* Quieten ALSA output messages


## [0.2.1](https://github.com/ludouzi/fooyin/releases/tag/v0.2.0) (2024-01-23)

* Fix crash when a library scan detects new and changed tracks


## [0.2.0](https://github.com/ludouzi/fooyin/releases/tag/v0.2.0) (2024-01-23)

### Features
* Add gapless playback option
* Add column alignment options to Library Filter

### Changes

* Tracks not associated with a library are no longer added to several widgets
* Stop playback on next track if playlist is removed
* Clear all active filters when searching Library Filters
* Remove all active filters when resetting Library Filters
* Add newly found directories to LibraryWatcher

### Fixes

* Fix playlist header reporting an incorrect section count
* Fix a race condition when reporting library scan results
* Correctly mark all tracks not found missing
* Update library tracks if they were previously not found


## [0.1.0](https://github.com/ludouzi/fooyin/releases/tag/v0.1.0) (2024-01-21)

* Initial release of fooyin.
