# Changelog

## [0.3.0](https://github.com/ludouzi/fooyin/releases/tag/v0.2.0) (2024-01-26)

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