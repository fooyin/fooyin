# Changelog

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