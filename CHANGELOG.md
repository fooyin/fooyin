# Changelog

# Unreleased

### New Features

* Spectrum
  - Add a spectrum visualisation plugin ([#52](https://github.com/fooyin/fooyin/issues/52), [#1025](https://github.com/fooyin/fooyin/issues/1025), [#1238](https://github.com/fooyin/fooyin/pull/1238))

### Improvements

* Audio/Playback
  - Improve gapless playback handling ([#1248](https://github.com/fooyin/fooyin/issues/1248))
* Interface
  - Add device refresh support ([#1225](https://github.com/fooyin/fooyin/issues/1225))
  - Hide the checked frame for flat tool buttons
* VU Meter
  - Add peak falloff support and align peak decay with spectrum behaviour
  - Improve configuration with peak, legend, and gradient controls

### Fixes

* Audio/Playback
  - Fix repeat-track handling for the same logical segment ([#1232](https://github.com/fooyin/fooyin/issues/1232))
* Filters
  - Fix filter copy grouping and preserve saved filter order ([#1252](https://github.com/fooyin/fooyin/issues/1252))
  - Ignore stale asynchronous search results ([#1235](https://github.com/fooyin/fooyin/issues/1235))
* Interface
  - Fix layout editing overlay geometry for containers
  - Keep inactive commands visible in the shortcut editor ([#1233](https://github.com/fooyin/fooyin/issues/1233))
  - Avoid stale track selection use in context actions ([#1237](https://github.com/fooyin/fooyin/issues/1237))
  - Scale playlist background pixmaps with device pixel ratio ([#1247](https://github.com/fooyin/fooyin/pull/1247))
* Lyrics
  - Avoid resetting editor text on Apply ([#1234](https://github.com/fooyin/fooyin/issues/1234))
  - Fix current line highlighting mutating the undo stack
* Scrobbling
  - Reset the timestamp when replaying the current track after stop ([#1217](https://github.com/fooyin/fooyin/pull/1217))

### Build/System

* CI
  - Enable precompiled headers and ccache ([#1214](https://github.com/fooyin/fooyin/pull/1214))
* Packaging
  - Remove unnecessary executable bits ([#1213](https://github.com/fooyin/fooyin/pull/1213))
* Translations
  - Update translations and translation sources ([#1250](https://github.com/fooyin/fooyin/pull/1250), [#1251](https://github.com/fooyin/fooyin/pull/1251))

### Dev/API
* Plugin/Widget API
  - Expose the selected playlist to GUI plugins ([#1222](https://github.com/fooyin/fooyin/pull/1222))
  - Add copy-specific layout serialisation
  - Add special value text support to Advanced settings


## [0.10.8](https://github.com/fooyin/fooyin/releases/tag/v0.10.8) (2026-05-21)

### New Features

* Interface
  - Add `Now Playing` output functionality ([#1084](https://github.com/fooyin/fooyin/issues/1084), [#1194](https://github.com/fooyin/fooyin/pull/1194))
  - Add playlist background image options

### Improvements

* Interface
  - Move the Preserve timestamps option to Advanced settings
  - Command Button: Highlight toggleable actions and add checkable states for mute and menu bar actions
  - Playlist: Select the next track after removing or cutting a track ([#1177](https://github.com/fooyin/fooyin/issues/1177))
  - Add support for custom placeholder artwork
  - Add an option to expand or collapse Library Tree nodes with a single click ([#1195](https://github.com/fooyin/fooyin/issues/1195))
  - Add `Add to current playlist and play if stopped` actions for filters, the Library Tree, and the Directory Browser ([#1195](https://github.com/fooyin/fooyin/issues/1195))
  - Add a playlist context menu action for opening playlist settings
  - Add configuration for the Selection Info properties tab and ReplayGain widget
  - Improve settings page layouts, section headers, and script input fields
  - Add support for compact DSP layout widgets and SoundTouch layout editors
* Library/Metadata
  - Centralise artwork loading, request ordering, and thumbnail caching in a shared cover repository ([#784](https://github.com/fooyin/fooyin/issues/784))
  - Improve ID3v2.3 multivalue tag compatibility, including optional semicolon splitting for compatible fields ([#739](https://github.com/fooyin/fooyin/issues/739), [#1109](https://github.com/fooyin/fooyin/issues/1109), [#1129](https://github.com/fooyin/fooyin/issues/1129))
  - Add FFmpeg support for TAK files and APEv2 tag and cover art reading ([#764](https://github.com/fooyin/fooyin/issues/764), [#1129](https://github.com/fooyin/fooyin/issues/1129))
* Media Controls
  - Send embedded track lyrics over MPRIS with `xesam:asText` ([#1192](https://github.com/fooyin/fooyin/issues/1192))
* Scripting
  - Add `%_fooyin_version%`, `%isstopped%` and `%datetime%`

### Fixes

* Audio/Playback
  - Fix DSP manager re-adding removed pending DSPs when applying changes ([#1176](https://github.com/fooyin/fooyin/issues/1176))
  - Fix playback engine shutdown ordering
  - Fix fade-pause timing ([#1183](https://github.com/fooyin/fooyin/issues/1183))
  - Fix progress/time listened not being counted when replaying the current track from a stopped state ([#1202](https://github.com/fooyin/fooyin/issues/1202))
* Interface
  - Save shortcut changes on Apply/OK instead of shutdown ([#1186](https://github.com/fooyin/fooyin/issues/1186))
  - Fix themed playback icons after restart ([#1174](https://github.com/fooyin/fooyin/issues/1174))
  - Refresh playlist controls and highlight icons after palette changes
  - Fix scrobbler toggle icon colours after theme refresh
  - Fix stale Search Controller widget connections after layout edits ([#1199](https://github.com/fooyin/fooyin/issues/1199))
  - Prevent track selection actions on auto playlists
  - Show Selection Info tooltips when text is elided ([#1206](https://github.com/fooyin/fooyin/pull/1206))
  - Fix file dimensions in artwork properties tab
  - Prevent the elapsed progress dialog from opening after completion
  - Refresh cover views after artwork cache invalidation ([#1211](https://github.com/fooyin/fooyin/issues/1211))
* Library/Metadata
  - Improve artist slash detection when reading metadata ([#1181](https://github.com/fooyin/fooyin/pull/1181))
  - Fix automatic rating scale detection for custom tags ([#1175](https://github.com/fooyin/fooyin/pull/1175))
  - Reject non-finite ReplayGain values ([#1196](https://github.com/fooyin/fooyin/pull/1196))
  - Fix writing of ID3 composer and performer tags ([#1207](https://github.com/fooyin/fooyin/issues/1207))
  - Fix duplicate ID3 track and disc total fields
  - Fix rating/playback statistics not persisting after library rescans ([#1212](https://github.com/fooyin/fooyin/issues/1212))
* Tag Editor
  - Fix changing mixed tags when the selected tracks share the same value
  - Fix doubled spaces in multivalue tag edits ([#1210](https://github.com/fooyin/fooyin/issues/1210))
* Playlist
  - Fix `Stop after this` when playback is stopped ([#1182](https://github.com/fooyin/fooyin/pull/1182))
  - Fix unique playlist name generation after playlists have been removed ([#1180](https://github.com/fooyin/fooyin/issues/1180))
  - Fix a potential crash when switching layouts from throttled signal emission during teardown
* PipeWire
  - Preserve PipeWire volume when recreating streams after sample rate changes ([#1178](https://github.com/fooyin/fooyin/issues/1178))
  - Stop syncing PipeWire stream volume and mute changes back to fooyin's player volume ([#1197](https://github.com/fooyin/fooyin/issues/1197))
* Scripting
  - Fix `$ascii` accepting Latin1 Supplement characters ([#1185](https://github.com/fooyin/fooyin/issues/1185), [#1188](https://github.com/fooyin/fooyin/pull/1188))
  - Fix `$replace` variadic arguments and empty string handling ([#1162](https://github.com/fooyin/fooyin/issues/1162))

### Build/System

* CI
  - Drop the Arch Linux CI job now that fooyin is available in Arch Linux extra
* Translations
  - Update translations and translation sources ([#1208](https://github.com/fooyin/fooyin/pull/1208), [#1209](https://github.com/fooyin/fooyin/pull/1209))


## [0.10.7](https://github.com/fooyin/fooyin/releases/tag/v0.10.7) (2026-05-15)

### New Features

* Quick Tagger
  - Add a Quick Tagger plugin for configurable tagging actions ([#836](https://github.com/fooyin/fooyin/issues/836), [#1153](https://github.com/fooyin/fooyin/pull/1153))

### Improvements

* DSP
  - Add `View -> Equaliser` option when an equaliser DSP is present in a chain
* Interface
  - Add track selection support to the Queue Viewer ([#318](https://github.com/fooyin/fooyin/issues/318))
  - Add `Add to Playlist` context menu actions to track selections, filters, the Library Tree, and the Directory Browser ([#831](https://github.com/fooyin/fooyin/issues/831))
  - Reorganise settings page categories and start categories collapsed by default
  - Show a placeholder for empty settings categories and simplify single-page category display
  - Replace Tab Stack cardinal direction labels with standard directions ([#1156](https://github.com/fooyin/fooyin/pull/1156))
  - Add a seekbar and WaveBar setting for whether mouse interaction takes focus ([#769](https://github.com/fooyin/fooyin/issues/769))
* Playlist
  - Add settings to ignore playlist files when adding folders and prevent duplicate entries when loading playlists ([#1160](https://github.com/fooyin/fooyin/issues/1160))
  - Remove the playback queue size limit
* FileOps
  - Move the single-operation confirmation setting to Advanced settings
* Lyrics
  - Improve the layout of font and colour settings
* Scripting
  - Add `$isalpha`, `$isalnum`, and `$isnum` string functions ([#982](https://github.com/fooyin/fooyin/issues/982), [#1168](https://github.com/fooyin/fooyin/pull/1168))
  - Preserve path separators in path variables ([#777](https://github.com/fooyin/fooyin/issues/777))
* Tag Editor
  - Add enabled and multi-value columns to tag editor fields
* WaveBar
  - Cache supersampled waveform renders
  - Add options to normalise waveforms, use a dB scale, and choose peak display mode

### Fixes

* Audio/Playback
  - Fix Stop After Current follow-up navigation ([#1154](https://github.com/fooyin/fooyin/issues/1154))
  - Fix queue follow navigation after queued playback
  - Avoid duplicate file size calculations for bounded segments ([#1161](https://github.com/fooyin/fooyin/issues/1161))
  - Refine CUE handling in external scans
* Filters
  - Fix filtered `All` selections using the whole library instead of matching entries ([#1151](https://github.com/fooyin/fooyin/issues/1151))
* Interface
  - Fix inline editors accepting changes after pressing Escape ([#1170](https://github.com/fooyin/fooyin/pull/1170))
  - Fix bottom viewport row selection repaint in tree views
  - Sync widget configuration state after applying
* Library/Metadata
  - Fix custom rating text tag settings and rating write mappings ([#1148](https://github.com/fooyin/fooyin/issues/1148), [#1150](https://github.com/fooyin/fooyin/issues/1150))
* PipeWire
  - Sync PipeWire volume and mute state with the system mixer ([#1147](https://github.com/fooyin/fooyin/issues/1147))
* Scrobbling
  - Make the scrobbling threshold independent of the playcount threshold ([#1152](https://github.com/fooyin/fooyin/issues/1152))
* Settings
  - Fix relative positioning of settings categories and category identity across translations
* VU Meter, WaveBar
  - Always set the active palette for the default highlight colour

### Build/System

* Packaging
  - Target Ubuntu 26.04 and update distro-specific dependencies ([#1171](https://github.com/fooyin/fooyin/issues/1171))
* Translations
  - Update translations and translation sources ([#1164](https://github.com/fooyin/fooyin/pull/1164), [#1165](https://github.com/fooyin/fooyin/pull/1165), [#1172](https://github.com/fooyin/fooyin/pull/1172), [#1173](https://github.com/fooyin/fooyin/pull/1173))


## [0.10.6](https://github.com/fooyin/fooyin/releases/tag/v0.10.6) (2026-05-11)

### Improvements

* Filters
  - Add an option to change cover source preference ([#1021](https://github.com/fooyin/fooyin/issues/1021), [#1111](https://github.com/fooyin/fooyin/pull/1111), [#1124](https://github.com/fooyin/fooyin/issues/1124))
* Interface
  - Add the Tag Editor as a layout widget ([#1012](https://github.com/fooyin/fooyin/pull/1012))
  - Add filter for searching shortcuts and improve the shortcut editor
  - Add support for selecting built-in theme icons in Command Button widgets
  - Add an unsplit/remove split action for single-child layout widget containers ([#1132](https://github.com/fooyin/fooyin/issues/1132))
  - Add an Advanced settings page and move technical settings there
  - Move plugin settings pages under the Plugins category
  - Make the layout editing context menu configurable
  - Save and restore settings dialog page state and expanded categories
  - Improve layout editing context menus and menu header appearance
  - Split playlist settings into focused pages
  - Add a separate action for clearing the current playlist
  - Add autocomplete to inline playlist editing ([#1136](https://github.com/fooyin/fooyin/issues/1136))
* Library/Metadata
  - Add configurable rating tag read/write handling for text tags and ID3 POPM frames ([#370](https://github.com/fooyin/fooyin/issues/370), [#786](https://github.com/fooyin/fooyin/issues/786), [#1120](https://github.com/fooyin/fooyin/pull/1120))
  - Add support for multi-chapter files ([#694](https://github.com/fooyin/fooyin/issues/694), [#945](https://github.com/fooyin/fooyin/pull/945))
* Lyrics
  - Support finding local lyrics with directory wildcards
* Playlist
  - Add setting to load directory when opening a single file ([#841](https://github.com/fooyin/fooyin/pull/841))
* Scripting
  - Add date and time functions: `$year`, `$month`, `$day_of_month`, `$date`, and `$time`
  - Add `$meta(field,index)`, `$meta_sep`, `$meta_test`, and `$meta_num`
  - Fall back to literal search when query syntax is invalid ([#1125](https://github.com/fooyin/fooyin/issues/1125))
* Tag Editor
  - Add support for configuring multivalue separators ([#1141](https://github.com/fooyin/fooyin/issues/1141))

### Fixes

* Audio/Playback
  - Respect current track seekability in playback controls and WaveBar
  - Handle seek trimming for all formats in the FFmpeg decoder
  - Fix several playback queue issues after session restore ([#1130](https://github.com/fooyin/fooyin/pull/1130))
  - Fix potential incorrect shuffle order ([#1127](https://github.com/fooyin/fooyin/issues/1127))
* Discord
  - Support sandboxed IPC socket locations ([#1115](https://github.com/fooyin/fooyin/issues/1115))
* Interface
  - Update the Script Editor track when the selection changes ([#1122](https://github.com/fooyin/fooyin/issues/1122))
  - Fix playlist header selection deleting tracks
  - Fall back to the default or first playlist when restoring the active playlist fails
  - Fix Library Tree sorting and searching when strings contain scripting syntax ([#1117](https://github.com/fooyin/fooyin/issues/1117), [#1125](https://github.com/fooyin/fooyin/issues/1125))
  - Fix Library Tree row height with multiline titles
  - Fix the cursor moving to the first row when adding new rows in extendable tables
* Library/Metadata
  - Fix swapped Encoding and TagType values
  - Fix FFmpeg tag decoding issues, including semicolon/slash-separated artists and encoder tool info ([#1137](https://github.com/fooyin/fooyin/pull/1137))
  - Fix missing ReplayGain values evaluating as true in scripts ([#1112](https://github.com/fooyin/fooyin/issues/1112))
* Lyrics
  - Fix LRC offset calculation ([#1121](https://github.com/fooyin/fooyin/issues/1121))
* Playlist
  - Fix 'Cursor follows playback' when restoring state on startup
* Scripting
  - Fix `NOT` parsing for date range queries ([#1143](https://github.com/fooyin/fooyin/issues/1143))
  - Fall back to literal search when query syntax is invalid ([#1125](https://github.com/fooyin/fooyin/issues/1125))
  - Fix `$rand` bounds ([#1133](https://github.com/fooyin/fooyin/pull/1133))

### Build/System

* Build
  - Add a `PFFFT_USE_SIMD` CMake option for the PFFFT library ([#1110](https://github.com/fooyin/fooyin/issues/1110))
* Translations
  - Mark file extension hint labels translatable ([#1108](https://github.com/fooyin/fooyin/issues/1108))
  - Update translations and translation sources ([#1134](https://github.com/fooyin/fooyin/pull/1134), [#1135](https://github.com/fooyin/fooyin/pull/1135))


## [0.10.5](https://github.com/fooyin/fooyin/releases/tag/v0.10.5) (2026-05-04)

### Improvements

* Interface
  - Add support for wildcard artwork directory paths ([#1099](https://github.com/fooyin/fooyin/issues/1099))
  - Split playlist and queue context-menu actions into separate configurable actions
  - Improve playlist header appearance when artwork is hidden
  - Add a playlist preset menu option for opening the preset settings page
* Directory Browser
  - Add search support ([#1106](https://github.com/fooyin/fooyin/issues/1106))
  - Make context menu configurable
* Library Tree
  - Add support for right-aligned text with `<right>`
  - Add configurable summary node script
  - Add `%trackcount%`, `%childcount%`
* Scripting
  - Add support for preserving layout whitespace during evaluation

### Fixes

* Audio/Playback
  - Refresh active stream metadata on track updates so ReplayGain changes apply immediately ([#1102](https://github.com/fooyin/fooyin/issues/1102))
* Interface
  - Fix editable tab middle-click handling
* Library/Metadata
  - Prefer ID3v2.4 `TDRC` over `TYER` when reading date tags ([#787](https://github.com/fooyin/fooyin/issues/787), [#1098](https://github.com/fooyin/fooyin/pull/1098))
  - Fix autoplaylist refresh and playlist changesets for custom tag changes ([#860](https://github.com/fooyin/fooyin/issues/860))
* Lyrics
  - Avoid stale editor and widget updates after the edited track changes ([#748](https://github.com/fooyin/fooyin/issues/748), [#1101](https://github.com/fooyin/fooyin/pull/1101))

### Build/System

* Packaging
  - Add zlib as an explicit dependency and Debian packaging dependency
  - Drop Fedora 41 and add 44
* Plugin API
  - Remove the legacy plugin settings API
* Translations
  - Update translations and translation sources ([#1104](https://github.com/fooyin/fooyin/pull/1104), [#1105](https://github.com/fooyin/fooyin/pull/1105))


## [0.10.4](https://github.com/fooyin/fooyin/releases/tag/v0.10.4) (2026-05-02)

### New Features

* Interface
  - Add `Playlist Manager`
  - Add configurable context menu settings with ordering and separators for track, filter, and Library Tree menus ([#495](https://github.com/fooyin/fooyin/issues/495))
  - Add inline metadata editing in playlist columns ([#684](https://github.com/fooyin/fooyin/issues/684), [#846](https://github.com/fooyin/fooyin/issues/846), [#1010](https://github.com/fooyin/fooyin/pull/1010))
  - Add a Properties dialog sidebar for multi-track editing ([#758](https://github.com/fooyin/fooyin/issues/758), [#991](https://github.com/fooyin/fooyin/issues/991), [#1005](https://github.com/fooyin/fooyin/pull/1005))
  - Add a standalone Playback Queue window from the View menu ([#1051](https://github.com/fooyin/fooyin/issues/1051))
* Playback
  - Add random track and album skip playback commands ([#1054](https://github.com/fooyin/fooyin/issues/1054))
* Tag Editor
  - Add `Automatically fill fields` tool ([#400](https://github.com/fooyin/fooyin/issues/400), [#837](https://github.com/fooyin/fooyin/issues/837))

### Improvements

* Audio/Playback
  - Add manual crossfade support when going to the previous track
  - Improve SDL output handling
  - Handle Opus header gain and ReplayGain ([#449](https://github.com/fooyin/fooyin/issues/449), [#1029](https://github.com/fooyin/fooyin/pull/1029))
* Discord
  - Add `Clear on pause` option ([#811](https://github.com/fooyin/fooyin/issues/811))
* Filters
  - Speed up icon layout size hints
* Interface
  - Improve Library Tree performance for large libraries
  - Improve large playlist loading and selection performance ([#1053](https://github.com/fooyin/fooyin/issues/1053))
  - Add multiline rich text rendering support across playlist, filter, and status widgets
  - Add a copy action to Selection Info ([#1072](https://github.com/fooyin/fooyin/issues/1072))
  - Add `Copy file location` and `Copy directory path` track actions ([#496](https://github.com/fooyin/fooyin/issues/496), [#1020](https://github.com/fooyin/fooyin/pull/1020))
  - Add an option to show all tracks when a search is empty ([#848](https://github.com/fooyin/fooyin/issues/848), [#1002](https://github.com/fooyin/fooyin/pull/1002))
  - Add context menu actions to the Playlist Switcher ([#985](https://github.com/fooyin/fooyin/issues/985))
  - Add settings actions to artwork and status widget context menus
  - Add an option for the Status Widget to show current playlist information
  - Add configurable artwork thumbnail grouping ([#1082](https://github.com/fooyin/fooyin/issues/1082))
  - Split Library Tree and WaveBar settings into tabs to fit smaller displays ([#1062](https://github.com/fooyin/fooyin/issues/1062))
  - Fix missing updates when inserting rows into hidden views ([#983](https://github.com/fooyin/fooyin/issues/983))
* Library/Metadata
  - Serialise library scan commits and defer completion until changes are applied
  - Normalise track ratings when writing to the database
  - Cache metadata writability checks by extension ([#1053](https://github.com/fooyin/fooyin/issues/1053))
* FileOps
  - Add support for extracting archive entries
* Lyrics
  - Add a Kugou lyrics source and word-by-word lyrics support for NetEase ([#1008](https://github.com/fooyin/fooyin/pull/1008))
  - Add negative synced-lyrics offset support ([#975](https://github.com/fooyin/fooyin/pull/975))
  - Rework lyrics editing and saving across the widget and Properties dialog
  - Add a manual search dialog and additional context actions
* Playlist
  - Add support for sort scripts in playlist columns
  - Add sorting options to Playlist Organiser ([#810](https://github.com/fooyin/fooyin/pull/810))
  - Add left/right display scripting for Playlist Organiser ([#1040](https://github.com/fooyin/fooyin/issues/1040))
  - Improve playing row colour ([#989](https://github.com/fooyin/fooyin/issues/989))
* Scripting
  - Add `%playlist_size%` and boolean functions `$and`, `$or`, `$not`, and `$xor`

### Fixes

* Audio/Playback
  - Avoid restoring playback state on startup when playback is stopped ([#980](https://github.com/fooyin/fooyin/issues/980))
  - Preserve shuffle history during next/previous navigation ([#1034](https://github.com/fooyin/fooyin/issues/1034))
  - Avoid unnecessary DSP chain output reinit on stale format predictions
  - Preserve the restored active playlist track on shutdown
  - Fix paused position synchronisation after seeking to the track start ([#1088](https://github.com/fooyin/fooyin/issues/1088))
  - Fix fade-in-only pause resume handling ([#1085](https://github.com/fooyin/fooyin/issues/1085))
  - Handle FFmpeg errors when interleaving planar samples ([#1091](https://github.com/fooyin/fooyin/issues/1091))
* Filters
  - Enable plain-text search matching in comment and custom metadata fields ([#776](https://github.com/fooyin/fooyin/issues/776), [#975](https://github.com/fooyin/fooyin/pull/975))
  - Rewrite grouped filter state handling, refresh row heights after restoring view state, and fix the `Show header` toggle ([#964](https://github.com/fooyin/fooyin/issues/964), [#1001](https://github.com/fooyin/fooyin/issues/1001), [#1004](https://github.com/fooyin/fooyin/pull/1004))
  - Fix `All` selections including non-library tracks ([#1066](https://github.com/fooyin/fooyin/issues/1066))
  - Refresh widget fonts on runtime font changes
* Interface
  - Fix X11 desktop icon startup class handling ([#959](https://github.com/fooyin/fooyin/issues/959), [#975](https://github.com/fooyin/fooyin/pull/975))
  - Fix status bar selection elision and multiline right alignment ([#1049](https://github.com/fooyin/fooyin/pull/1049))
  - Fix the base theme font not being applied in `StatusWidget` ([#1011](https://github.com/fooyin/fooyin/pull/1011))
  - Rename the mislabelled WaveBar remaining-time option ([#1017](https://github.com/fooyin/fooyin/issues/1017), [#1019](https://github.com/fooyin/fooyin/pull/1019))
  - Use the hovered tab when renaming tabs in tab stacks ([#1063](https://github.com/fooyin/fooyin/pull/1063))
  - Restore active playlist artwork in the Cover Widget
  - Restore the saved proxy type in network settings ([#1077](https://github.com/fooyin/fooyin/issues/1077))
  - Fix Script Display copy shortcut handling ([#1078](https://github.com/fooyin/fooyin/issues/1078))
  - Improve editable tab right-click and middle-click handling
* Lyrics
  - Fix rich-text formatting in `No lyrics script` ([#1037](https://github.com/fooyin/fooyin/issues/1037))
  - Make synced-lyrics edge centering configurable ([#1056](https://github.com/fooyin/fooyin/issues/1056))
  - Fix trailing word timings and a crash when saving lyrics with autosave enabled ([#1038](https://github.com/fooyin/fooyin/issues/1038))
* Library/Metadata
  - Fix MP4/AAC ReplayGain parsing for gain strings with dB suffixes ([#1048](https://github.com/fooyin/fooyin/issues/1048), [#1050](https://github.com/fooyin/fooyin/pull/1050))
  - Prefer FMPS statistics when reading tags ([#1068](https://github.com/fooyin/fooyin/issues/1068))
  - Improve reading of MPEG files with multiple tag formats ([#1090](https://github.com/fooyin/fooyin/issues/1090))
* Notifications
  - Query notification capabilities asynchronously ([#1087](https://github.com/fooyin/fooyin/issues/1087))
* Playlist
  - Rework the playlist model around stable entry ids to keep playback, queue, undo, and now-playing state consistent across edits ([#1034](https://github.com/fooyin/fooyin/issues/1034), [#1039](https://github.com/fooyin/fooyin/issues/1039))
  - Fix `Cursor follows playback` during engine-owned automatic transitions
  - Fix stale playlist tracks after FileOps rename and move operations ([#1052](https://github.com/fooyin/fooyin/issues/1052))
  - Fix range selection when shift-clicking playlist headers ([#1067](https://github.com/fooyin/fooyin/issues/1067))
  - Fix potential crash when reparenting children during header merges ([#1061](https://github.com/fooyin/fooyin/issues/1061))
* Scrobbling
  - Fix Last.fm submission signing and cache recovery for invalid entries ([#999](https://github.com/fooyin/fooyin/issues/999))
  - Fix ListenBrainz HTTP 400 failures from cached invalid MBIDs and stop submitting when a service is disabled or unauthenticated ([#998](https://github.com/fooyin/fooyin/issues/998), [#996](https://github.com/fooyin/fooyin/issues/996))
  - Fix Libre.fm authentication and scrobbling
  - Fix authentication callback handling
* Scripting
  - Evaluate scripts even with an empty track list
* Tag Editor
  - Fix copy and paste actions; use cell selection ([#1083](https://github.com/fooyin/fooyin/pull/1083))
* VU Meter
  - Fix static layer scaling on high-DPI displays ([#1075](https://github.com/fooyin/fooyin/pull/1075))

### Build/System
* CI/Release
  - Add macOS `.dmg` packaging ([#579](https://github.com/fooyin/fooyin/pull/579), [#1016](https://github.com/fooyin/fooyin/pull/1016))
  - Build Windows ARM artifacts ([#1028](https://github.com/fooyin/fooyin/pull/1028))
  - Add Arch Linux build artifacts
* Translations
  - Update translations and translation sources ([#1095](https://github.com/fooyin/fooyin/pull/1095), [#1096](https://github.com/fooyin/fooyin/pull/1096))
  - Add desktop comment translation ([#1097](https://github.com/fooyin/fooyin/pull/1097))


## [0.10.3](https://github.com/fooyin/fooyin/releases/tag/v0.10.3) (2026-04-02)

### New Features

* Interface
  - Add an option for tab stacks to remember and restore the last selected tab
* Notifications
  - Add playback controls and a portal fallback to desktop notifications

### Improvements

* Filters
  - Avoid eager cover loading in icon mode

### Fixes

* Audio/Playback
  - Fix ALSA device disconnection handling regression ([#962](https://github.com/fooyin/fooyin/issues/962))
  - Fix prepared-track fallback after format changes, so the next track starts from the
    beginning ([#967](https://github.com/fooyin/fooyin/issues/967))
  - Fix duplicate error dialogs for failed track loads
  - Fix playcount incrementing after seeking to the end of the last track
  - Preserve shuffle history on track updates and improve previous-track
    availability ([#968](https://github.com/fooyin/fooyin/issues/968))
* Filters
  - Ignore stale filter population results after resets ([#964](https://github.com/fooyin/fooyin/issues/964))
  - Fix poor performance by avoiding eager cover loading in icon mode
* Interface
  - Fix seek label resizing so WaveBar layouts stay stable ([#966](https://github.com/fooyin/fooyin/issues/966))
  - Improve artwork placeholder sizing and avoid repeated synchronous no-cover retries
  - Restore `?` grouping and playback markers for metadata-less tracks in Library
    Tree ([#958](https://github.com/fooyin/fooyin/issues/958))
  - Fix Library Tree playback tracking for empty group titles
* Lyrics
  - Preserve lyrics on track updates ([#970](https://github.com/fooyin/fooyin/issues/970))
* Library/Metadata
  - Restrict MP4 artwork support to front covers only ([#963](https://github.com/fooyin/fooyin/issues/963))
  - Fix a potential library scan crash caused by extra-tag loading ([#958](https://github.com/fooyin/fooyin/issues/958))
* MPRIS
  - Fix cached album artwork being shown ([#968](https://github.com/fooyin/fooyin/issues/968))
* SoX Resampler
  - Resolve a memory leak

## [0.10.2](https://github.com/fooyin/fooyin/releases/tag/v0.10.2) (2026-03-30)

### New Features
* FileOps
  - Add an action and shortcut to delete tracks ([#932](https://github.com/fooyin/fooyin/pull/932))
* Notifications
  - Add track change desktop notifications ([#801](https://github.com/fooyin/fooyin/pull/801))
* Playback
  - Add per-device output profiles and an output selector ([#947](https://github.com/fooyin/fooyin/pull/947))

### Improvements
* Build/System
  - Add CMake support for selecting which plugins to build ([#941](https://github.com/fooyin/fooyin/issues/941))
* Interface
  - Add a Playback Statistics section to selection info ([#942](https://github.com/fooyin/fooyin/issues/942))
  - Add a new Track Display settings page with configurable full, half, and empty star symbols for `%rating_stars%` and `%rating_stars_padded%` ([#938](https://github.com/fooyin/fooyin/issues/938))
  - Support extracting artwork for archive tracks
  - Add waveform render supersampling, and increase the max number of samples per channel to 8192 ([#897](https://github.com/fooyin/fooyin/issues/897))
* Library/Metadata
  - Add ReplayGain parsing in FFmpeg, improve ReplayGain string handling, and improve audio bitdepth reporting ([#946](https://github.com/fooyin/fooyin/issues/946))
  - Improve library scan completion handling, post summary status messages, and guard sort completions from stale overwrites ([#953](https://github.com/fooyin/fooyin/issues/953))
  - Treat CUE `PERFORMER` as artist when tracks omit it ([#940](https://github.com/fooyin/fooyin/issues/940))
* Playlist
  - Improve Playlist Organiser drag-and-drop handling
* Scripting
  - Make contains queries accent-insensitive ([#957](https://github.com/fooyin/fooyin/issues/957))

### Fixes
* Audio/Playback
  - Fix PipeWire lifecycle issues during output switching and device enumeration
  - Reinitialise playback buffering when `BufferLength` changes during playback
  - Fix output volume being applied twice after fades ([#944](https://github.com/fooyin/fooyin/issues/944))
  - Fix DSP preset save/load after removing DSPs
  - Fix playback queue follow-up using the queued track playlist ([#952](https://github.com/fooyin/fooyin/issues/952))
* Interface
  - Fix exported file naming in save dialogs and avoid portal-backed save target issues ([#937](https://github.com/fooyin/fooyin/issues/937))
  - Fix translations for ScriptEditor references and the Interface settings page ([#935](https://github.com/fooyin/fooyin/issues/935))
  - Fix seeking in faded areas of synced lyrics
  - Fix leaked `ExtendableTableView` shortcut actions
  - Fix WaveBar rescaling double counting and RMS calculation
  - Fix stale state in `ExpandedTreeView` after model resets ([#956](https://github.com/fooyin/fooyin/issues/956))
  - Fix artwork cache clearing so playlist thumbnails do not stay cached as no-cover
* Integration
  - Fix stale MPRIS play/pause capabilities after playback starts ([#955](https://github.com/fooyin/fooyin/issues/955))
* Library/Metadata
  - Ignore empty embedded cover data when loading artwork
* Playlist
  - Fix mixed playlist/file drops losing standalone tracks
  - Fix right-click selection in Playlist Organiser
* Tag Editor
  - Fix a crash when opening the tag editor on some systems ([#943](https://github.com/fooyin/fooyin/issues/943))
  - Fix ratings for archive and CUE tracks
  - Improve metadata and artwork save failure reporting, and log artwork decode errors ([#939](https://github.com/fooyin/fooyin/issues/939))
* Testing
  - Isolate time-sensitive audio tests to reduce load-sensitive failures ([#934](https://github.com/fooyin/fooyin/issues/934))


## [0.10.1](https://github.com/fooyin/fooyin/releases/tag/v0.10.1) (2026-03-27)

### Fixes

* Equaliser: Fix build failure on Fedora


## [0.10.0](https://github.com/fooyin/fooyin/releases/tag/v0.10.0) (2026-03-27)

### New Features
* Discord
  - Add Rich Presence integration ([#94](https://github.com/fooyin/fooyin/issues/94), [#715](https://github.com/fooyin/fooyin/pull/715))
* DSP
  - Add DSP plugin support and a built-in DSP suite including `Skip Silence`, `Resampler (FFmpeg)`, `Convert mono to stereo`, `Reverse stereo channels`, `Downmix to stereo`, and `Downmix to mono`
  - Add SoundTouch-based DSPs ([#866](https://github.com/fooyin/fooyin/pull/866))
  - Add a SoX-based resampler DSP ([#880](https://github.com/fooyin/fooyin/pull/880))
* Lyrics
  - Add configurable edge fading and a per-instance base line font override
* Equaliser
  - Add a SuperEQ-based equaliser DSP ([#283](https://github.com/fooyin/fooyin/issues/283), [#415](https://github.com/fooyin/fooyin/issues/415), [#867](https://github.com/fooyin/fooyin/pull/867))
* Interface
  - Add per-widget configuration for built-in widgets and supported plugins ([#885](https://github.com/fooyin/fooyin/pull/885))
  - Add a new `CommandButton` widget
  - Add a new Script Display widget ([#357](https://github.com/fooyin/fooyin/issues/357), [#410](https://github.com/fooyin/fooyin/issues/410), [#844](https://github.com/fooyin/fooyin/issues/844), [#889](https://github.com/fooyin/fooyin/pull/889))
  - Add support for viewing artwork full size, exporting/extracting artwork, and loading original-size covers ([#791](https://github.com/fooyin/fooyin/issues/791), [#835](https://github.com/fooyin/fooyin/issues/835))
* Playback
  - Add configurable fading and crossfading for seek, manual track changes, and automatic track transitions ([#123](https://github.com/fooyin/fooyin/issues/123), [#403](https://github.com/fooyin/fooyin/issues/403), [#884](https://github.com/fooyin/fooyin/pull/884))
* Playlist
  - Add playback queue persistence between sessions
  - Add CUE support when saving and loading M3U playlists ([#700](https://github.com/fooyin/fooyin/issues/700))
  - Rework autoplaylists to support incremental updates ([#830](https://github.com/fooyin/fooyin/issues/830), [#847](https://github.com/fooyin/fooyin/issues/847), [#860](https://github.com/fooyin/fooyin/issues/860), [#895](https://github.com/fooyin/fooyin/pull/895))
* Scripting
  - Add `$if3`, `$get`, `$put`, and `$puts`
  - Add `%createdtime%` and store track file creation time when available ([#821](https://github.com/fooyin/fooyin/issues/821), [#930](https://github.com/fooyin/fooyin/pull/930))
  - Add playlist-based variables to window title scripts ([#890](https://github.com/fooyin/fooyin/issues/890))
  - Add richer formatting support across script editing and display tools

### Improvements
* CLI
  - Add seek commands ([#760](https://github.com/fooyin/fooyin/pull/760))
* Directory Browser
  - Add options to show hidden files/directories and symlinks ([#850](https://github.com/fooyin/fooyin/pull/850))
* Engine
  - Complete engine rewrite ([#858](https://github.com/fooyin/fooyin/pull/858))
  - Add configurable decode buffer watermarks ([#869](https://github.com/fooyin/fooyin/issues/869))
  - Add manual override of output bit depth
* Interface
  - Add mouse forward/back support for playback controls ([#925](https://github.com/fooyin/fooyin/pull/925))
  - Add a local artwork source preference option to choose between embedded artwork and local files ([#657](https://github.com/fooyin/fooyin/issues/657))
  - Add rich text/script formatting support to `Status Widget`, `Queue Viewer`, `Library Tree`, `Filters`, and related displays
  - Split general settings into separate layout and display pages
  - Add conflict detection for shortcuts and validation error handling for settings pages
  - Add configure actions for decoders and tag readers that expose plugin settings
  - Resolve widget colours from the active palette and react better to icon theme/style changes
  - Allow editing default script-based items in widget registries ([#879](https://github.com/fooyin/fooyin/issues/879))
  - Add CoverWidget fading options and configuration
  - Add a dedicated Playback > Fading settings page for fade and crossfade options
  - Improve status widget double-click behaviour to jump to current playback context ([#808](https://github.com/fooyin/fooyin/pull/808))
  - Reposition playlist tab remove action ([#809](https://github.com/fooyin/fooyin/issues/809))
  - Improve seek label sizing to avoid clipped text
  - Improve icon-mode resize performance in tree and filter views
  - Improve supported-extension tooltips for decoders and tag readers
  - Use clearer names for connected-widget actions
  - Make rating stars respect the active style and selection state
  - Move waveform seekbar to the Visualisations menu
  - Show WaveBar cursor while paused
  - Improve WaveBar precision and gradient rendering
  - Add VU meter FPS and legend colour settings, and cache rendering
* Library/Metadata
  - Add support for embedding `webp`, `bmp`, and `gif` artwork ([#855](https://github.com/fooyin/fooyin/issues/855))
  - Handle tags `ARTIST` and `ARTISTS` separately ([#783](https://github.com/fooyin/fooyin/issues/783))
  - Improve archive entry metadata handling and preserve entry timestamps where available
  - Preserve rating, playcount, and played timestamps on file reload, with settings to control overwrite behaviour
  - Improve metadata read/write handling and tag editing workflows
  - Improve library scanning and batch progress updates ([#900](https://github.com/fooyin/fooyin/pull/900))
* Lyrics
  - Rewrite lyrics list/editor as `QListView` ([#818](https://github.com/fooyin/fooyin/pull/818))
  - Add quick action to update current line and move to the next ([#770](https://github.com/fooyin/fooyin/pull/770))
  - Add drag-seek and improve synced lyrics centering
  - Add multi-line highlighting and smoother synced-lyrics behaviour
  - Improve default played/unplayed colours, current line formatting, and word highlighting
  - Apply configured margins and alignment consistently in lyrics views and "no lyrics" display scripts
  - Adjust lyrics panel colours when the theme changes ([#875](https://github.com/fooyin/fooyin/pull/875))
  - Always save lyrics when applying editor changes
  - Respect hidden files when searching for local lyrics
  - Seek only on left-click ([#870](https://github.com/fooyin/fooyin/pull/870))
* Playlist
  - Add option to stop playback when queue finishes
  - Add action to remove unavailable tracks from the database
  - Add live evaluation of playback variables in playlists
  - Add external drag support for playlist tracks ([#927](https://github.com/fooyin/fooyin/pull/927))
  - Add a separate sort script option for library viewers ([#117](https://github.com/fooyin/fooyin/issues/117), [#321](https://github.com/fooyin/fooyin/issues/321))
  - Improve Library Tree performance, grouping, sorting, and playback-following behaviour
  - Add a `More...` option to the playlist sorting menu
  - Add queue actions to `Search Playlist` results ([#888](https://github.com/fooyin/fooyin/issues/888))
* GME
  - Enable `vgm`/`vgz` playback in GME
  - Make fading of non-looping tracks an opt-in setting ([#826](https://github.com/fooyin/fooyin/issues/826))
* Scrobbler
  - Submit cached scrobbles on startup
  - Send "Now Playing" updates at regular intervals ([#861](https://github.com/fooyin/fooyin/issues/861))
* Utilities
  - Add actions to copy log entries and full logs to clipboard
  - Queue log messages to avoid potential main-thread stalls
  - Use a dedicated `QCache` for cover caching

### Fixes
* Engine
  - Fix DSD playback stutters ([#853](https://github.com/fooyin/fooyin/issues/853))
  - Rework live bitrate reporting for more reliable updates ([#906](https://github.com/fooyin/fooyin/issues/906))
* Library/Metadata
  - Preserve inherited metadata when CUE fields are missing and handle trailing album metadata in CUE sheets ([#626](https://github.com/fooyin/fooyin/issues/626), [#756](https://github.com/fooyin/fooyin/issues/756))
  - Fix CUE source precedence, file matching, and duplicate resolution ([#677](https://github.com/fooyin/fooyin/issues/677))
* Playback
  - Fix several `Stop after this` edge cases ([#923](https://github.com/fooyin/fooyin/pull/923))
  - Fix "Send to playback queue" not starting playback
  - Fix playlist tabs only changing on click ([#693](https://github.com/fooyin/fooyin/issues/693), [#731](https://github.com/fooyin/fooyin/issues/731))
  - Fix Properties dialog not auto-defaulting to OK ([#752](https://github.com/fooyin/fooyin/pull/752))
  - Fix multi-column handling issues ([#761](https://github.com/fooyin/fooyin/pull/761))
* Formats/Decoders
  - Fix a double-free in the FFmpeg ReplayGain scanner backend ([#924](https://github.com/fooyin/fooyin/pull/924))
  - Fix FFmpeg misdetecting some FLAC files as MP3 ([#696](https://github.com/fooyin/fooyin/issues/696))
  - Reject non-audio inputs earlier in FFmpeg and fix crashes when attempting to index such files
  - Handle invalid audio properties more safely in TagLib and SndFile
  - Fix GME loop count handling ([#838](https://github.com/fooyin/fooyin/issues/838))
* Interface
  - Reduce artwork flashing during track changes ([#466](https://github.com/fooyin/fooyin/issues/466))
  - Fix `Status Widget` playing text not updating when playback is paused ([#922](https://github.com/fooyin/fooyin/pull/922))
  - Fix WaveBar getting stuck in the playing state after seeking during stop fade
  - Fix Properties/Artwork tab updates not propagating across tabs ([#894](https://github.com/fooyin/fooyin/issues/894))
  - Fix re-adding removed widgets without switching layouts ([#377](https://github.com/fooyin/fooyin/issues/377))
  - Fix `Directory Browser` control visibility restoration ([#893](https://github.com/fooyin/fooyin/pull/893))
  - Fix `Queue Viewer` not removing the current playback track when playback stops
  - Fix ReplayGain editor rounding, multi-value no-ops, and summary edit columns
  - Fix tag editor column sizing and restore behaviour ([#859](https://github.com/fooyin/fooyin/issues/859))
  - Fix horizontal scrolling artefacts and several view/layout regressions ([#909](https://github.com/fooyin/fooyin/pull/909), [#910](https://github.com/fooyin/fooyin/pull/910), [#914](https://github.com/fooyin/fooyin/pull/914))
* Integration
  - Fix MPRIS metadata typing, seek notifications, stale cover metadata, and temp cover cleanup ([#469](https://github.com/fooyin/fooyin/issues/469), [#670](https://github.com/fooyin/fooyin/issues/670), [#744](https://github.com/fooyin/fooyin/pull/744), [#896](https://github.com/fooyin/fooyin/pull/896))
  - Use the filename as an MPRIS title fallback when metadata is missing ([#745](https://github.com/fooyin/fooyin/issues/745))
  - Fix proxy settings persistence
* Platform
  - Fix GME MSVC build with Qt `6.10.1`
  - Fix Qt `6.10.1` compatibility issues ([#780](https://github.com/fooyin/fooyin/pull/780))
  - Fix missing VU meter include for `QElapsedTimer` ([#724](https://github.com/fooyin/fooyin/issues/724), [#725](https://github.com/fooyin/fooyin/pull/725))
  - Fix OGG detection on Windows in TagLib
  - Correct `StartupWMClass` metadata ([#839](https://github.com/fooyin/fooyin/pull/839))
* Core/Settings
  - Fix `%isplaying%` and `%ispaused%` script variables never evaluating to `false` ([#646](https://github.com/fooyin/fooyin/issues/646), [#707](https://github.com/fooyin/fooyin/pull/707))

### Build/System
* Build/CMake
  - Add QCoro dependency ([#714](https://github.com/fooyin/fooyin/pull/714))
  - Add `libqt6concurrent6` to Ubuntu/Debian package dependencies ([#778](https://github.com/fooyin/fooyin/issues/778))
  - Add optional SDK argument for custom `json.in`
  - Remove component from plugin install rule
* CI/Release
  - Add and refine release workflows
  - Add and refine translation/source update workflows
  - Add clang-format workflow and formatting checks
  - Build Linux arm64 binaries ([#800](https://github.com/fooyin/fooyin/pull/800))
  - Clean up packaging jobs and workflow actions
* Translations
  - Update translation sources and import latest Weblate changes

### Dev/API
* Core API
  - Drive repeat-track behaviour via decoder playback hints ([#878](https://github.com/fooyin/fooyin/issues/878))
  - Add `PlayerController` `positionChanged` and `bitrateChanged` signals
  - Add original-size cover loading support to `CoverProvider`
* Plugin/Widget API
  - Add `PluginConfigGuiPlugin` and `PluginSettingsProvider` for GUI-owned plugin configuration dialogs
  - Keep the legacy `Plugin::hasSettings()` / `Plugin::showSettings()` path as a fallback during migration
  - Add `ConfigDialog` and expand `FyWidget` to support per-instance widget configuration, saved defaults, and standard `Configure...` actions ([#885](https://github.com/fooyin/fooyin/pull/885))
* Repository
  - Remove in-tree `libvgm` plugin and submodule
  - Reorganise tests into subdirectories and update test utility include paths


## [0.9.2](https://github.com/fooyin/fooyin/releases/tag/v0.9.2) (2025-09-21)

### Improvements
* Core: Add option to preserve file timestamps ([#660](https://github.com/fooyin/fooyin/issues/660))
* FileOps
  - Add presets to track selection context menu
  - Remove writable check on directory ([#655](https://github.com/fooyin/fooyin/issues/655))
* GUI: Use helper for saving/restoring state in MainWindow
* GUI/MPRIS: Always load album covers from file ([#665](https://github.com/fooyin/fooyin/issues/665))
* Info Panel
  - Adjust value column size ([#342](https://github.com/fooyin/fooyin/issues/342))
  - Add option to toggle horizontal scrollbar ([#342](https://github.com/fooyin/fooyin/issues/342))
* OpenMPT: Add loop count option to settings ([#643](https://github.com/fooyin/fooyin/pull/643))
* Playlist
  - Add actions to remove duplicate and dead tracks ([#607](https://github.com/fooyin/fooyin/issues/607))
  - Add setting to control track preload count ([#571](https://github.com/fooyin/fooyin/issues/571))
* Scrobbler: Add support for custom services ([#672](https://github.com/fooyin/fooyin/pull/672))
* Lyrics
  - Add lyrics tab to properties dialog
  - Add forward and rewind buttons for precise timestamp adjustments ([#623](https://github.com/fooyin/fooyin/pull/623))
  - Allow removing lyrics
  - Improve layout of editor controls

### Fixes
* Engine
  - Drain audio output before closing ([#654](https://github.com/fooyin/fooyin/issues/654))
  - Fix playback of multi-track files ([#627](https://github.com/fooyin/fooyin/issues/627))
  - Fix regression with playback restarting on unpause
* GME/OpenMPT/LibVGM: Fix infinite looping ([#668](https://github.com/fooyin/fooyin/issues/668))
* Library: Fix parsing CUE sheets with multiple files/tracks ([#582](https://github.com/fooyin/fooyin/issues/582))
* Playback
  - Always follow track if option is enabled, regardless of playstate
  - Fix playback switching to queued playlist, even with 'Follow Playback Queue' disabled ([#647](https://github.com/fooyin/fooyin/issues/647))
* Playlist
  - Fix regression with playlist track selection behavior
  - Fix crash when active playlist is deleted ([#658](https://github.com/fooyin/fooyin/issues/658))
  - Fix highlighting playing track ([#664](https://github.com/fooyin/fooyin/issues/664))
  - Fix stop icon persisting
  - Fix scrolling to current index
  - Fix selection restoration ([#666](https://github.com/fooyin/fooyin/issues/666))
  - Limit cursor movement to track rows
* Scrobbler
  - Fix MusicBrainz token not being saved
  - Fix duration and listened_at types ([#676](https://github.com/fooyin/fooyin/issues/676))

### Build/System
* Build: Update to use C++23 ([#685](https://github.com/fooyin/fooyin/pull/685))
* Ubuntu/Debian: Fix package dependencies ([#620](https://github.com/fooyin/fooyin/pull/620))


## [0.9.1](https://github.com/fooyin/fooyin/releases/tag/v0.9.1) (2025-08-17)

### Fixes

* Engine: Fix silent playback in some cases ([#380](https://github.com/fooyin/fooyin/issues/380))
* Engine: Resolve crash when playing tracks in archives
* Directory Browser: Fix scrolling to top on playback ([#617](https://github.com/fooyin/fooyin/issues/617))
* Directory Browser: Fix restoring directory on startup in tree mode
* Scrobbler: Fix layout of settings page
* Seekbar: Fix display of labels in some instances
* Status Bar: Fix evaluation of playing script when active playlist tracks change
* WaveBar: Resolve crash when seeking ([#616](https://github.com/fooyin/fooyin/issues/616))


## [0.9.0](https://github.com/fooyin/fooyin/releases/tag/v0.9.0) (2025-08-17)

### New Features

* Artwork
  - Support changing embedded artwork
  - Support downloading and saving artwork ([#594](https://github.com/fooyin/fooyin/pull/594))
* Lyrics
  - Support finding, saving, and editing lyrics ([#355](https://github.com/fooyin/fooyin/pull/355))
* Playlist
  - Add autoplaylist functionality ([#366](https://github.com/fooyin/fooyin/pull/366))

### Improvements

* Artwork
  - CoverWidget: Always show individual track covers ([#525](https://github.com/fooyin/fooyin/issues/525))
  - Settings: Display disk cache usage and add button to clear
* Engine
  - Handle file changes during playback
  - Make VBR update interval configurable ([#375](https://github.com/fooyin/fooyin/issues/375))
  - Start playback on next/previous if stopped ([#564](https://github.com/fooyin/fooyin/issues/564))
  - Add support for audio/x-flac mime type ([#458](https://github.com/fooyin/fooyin/pull/458))
* Interface
  - Add setting to toggle main menu bar ([#534](https://github.com/fooyin/fooyin/issues/534))
  - LibraryTree: Align click actions with browser page ([#516](https://github.com/fooyin/fooyin/pull/516))
  - LibraryTree: Improve selection playlist behaviour
  - Selection Info: Include library name if track is in library
  - Settings: Add buttons to open config and share directories
* Playback
  - Add “Follow playback queue” option ([#479](https://github.com/fooyin/fooyin/pull/479))
* Playlist
  - Add `%list_index%` variable ([#519](https://github.com/fooyin/fooyin/issues/519))
  - Add queue/play next actions ([#367](https://github.com/fooyin/fooyin/issues/367))
  - Automatically load rest of playlist when ready ([#359](https://github.com/fooyin/fooyin/issues/359))
  - Optimise updating track data by limiting column updates ([#519](https://github.com/fooyin/fooyin/issues/519))
  - Perform searches by filtering the current playlist ([#350](https://github.com/fooyin/fooyin/issues/350))
  - Use alternating row colours in icon mode
* ReplayGain Scanner
  - Support changing scanner backend
* Scripting
  - Add `%playlist_elapsed%`, `%playlist_duration%` ([#457](https://github.com/fooyin/fooyin/issues/457))
  - Add `$round` ([#486](https://github.com/fooyin/fooyin/pull/486)), `$strstr` and friends ([#442](https://github.com/fooyin/fooyin/pull/442))
  - Add %filesize_natural% ([#417](https://github.com/fooyin/fooyin/pull/417))
  - Add `LIMIT` keyword and `*` alias for `ALL`
  - Add sorting shorthand `S+`/`S-` and improve sort expression evaluation
  - Add number arg to `$crlf` to support multiple line breaks in one call
* Scrobbler
  - Add filtering options ([#565](https://github.com/fooyin/fooyin/pull/565))
  - Add libre.fm service ([#517](https://github.com/fooyin/fooyin/issues/517))

### Fixes

* Engine
  - Fix duration of CUE tracks on initial read ([#376](https://github.com/fooyin/fooyin/issues/376))
  - Fix gain calculation with gapless playback enabled ([#402](https://github.com/fooyin/fooyin/issues/402))
  - Fix gapless playback with ReplayGain enabled ([#407](https://github.com/fooyin/fooyin/issues/407))
  - Fix playback of last track in CUE ([#385](https://github.com/fooyin/fooyin/issues/385))
  - Fix playback restarting when stopping with fade out ([#597](https://github.com/fooyin/fooyin/issues/597))
  - Fix seek interrupting fade-in ([#436](https://github.com/fooyin/fooyin/issues/436))
  - Fix silence between CUE albums ([#376](https://github.com/fooyin/fooyin/issues/376))
  - Fix playback of MP2 ([#612](https://github.com/fooyin/fooyin/issues/612))
* Interface
  - ExpandedTreeView: Fix crash with bottom captions in icon mode ([#437](https://github.com/fooyin/fooyin/issues/437))
  - ExpandedTreeView: Fix unintended scrolling after drop ([#542](https://github.com/fooyin/fooyin/issues/542))
  - Fix blurry icon in app switcher ([#599](https://github.com/fooyin/fooyin/issues/599))
  - Fix encoding for info symbol ([#419](https://github.com/fooyin/fooyin/issues/419))
  - Fix invisible playlist text in some cases ([#481](https://github.com/fooyin/fooyin/issues/481))
  - Fix layout export not respecting theme options
  - Fix layout export ignoring set layout name ([#557](https://github.com/fooyin/fooyin/pull/557))
  - Fix painting alternating row colours with some styles ([#583](https://github.com/fooyin/fooyin/issues/583))
  - Fix status widget scripts breaking on escaped reserved characters ([#514](https://github.com/fooyin/fooyin/issues/514))
* Library/LibraryScanner
  - Fix creation of garbage files when attempting to write metadata to moved files ([#604](https://github.com/fooyin/fooyin/pull/604))
  - Fix duplicate tracks when dropping CUE and related file
  - Improve detection of missing files (CUE) and log missing tracks
  - LibraryTree: Fix selection playlist not working after updating tracks
* Playlist
  - Fix changing rating
  - Fix drag-and-drop between playlists
  - Fix finding currently playing track
  - Fix saving state of hidden playlist ([#382](https://github.com/fooyin/fooyin/issues/382))
  - Fix crash in search dialog
  - Fix sorting by album ([#399](https://github.com/fooyin/fooyin/issues/399))
  - Fix underlying track order after sorting ([#358](https://github.com/fooyin/fooyin/issues/358))
  - Fix “Show playing track” after playlist change ([#358](https://github.com/fooyin/fooyin/issues/358))
  - Fix playlist auto-export not obeying settings ([#545](https://github.com/fooyin/fooyin/pull/545))
  - Fix import of multiple playlists merging into one ([#541](https://github.com/fooyin/fooyin/pull/541))
* Scrobbler
  - Fix ListenBrainz scrobbling ([#485](https://github.com/fooyin/fooyin/issues/485))
  - Prevent ListenBrainz sign-in without token
* WaveBar
  - Resolve crash when generating waveform of mono track ([#383](https://github.com/fooyin/fooyin/issues/383))



## [0.8.1](https://github.com/fooyin/fooyin/releases/tag/v0.8.1) (2024-10-15)

### Improvements

* LibraryScanner: Use a more detailed dialog that displays elapsed and estimated time
* LibraryScanner: Show per-file progress for archive and playlist scans
* RGScanner: Add dialog for removing RG info
* WaveBar: Use elapsed and estimated time dialog for generating waveforms

### Fixes

* Engine: Fix playback of WavPack
* RGScanner: Wait to close results dialog until files have been updated
* Settings: Fix reordering of decoders and tagreaders


## [0.8.0](https://github.com/fooyin/fooyin/releases/tag/v0.8.0) (2024-10-14)

### New Features

* Support for calculating ReplayGain ([#269](https://github.com/fooyin/fooyin/pull/269))
  - Uses libebur128 if found, or FFmpeg as a fallback
* Quick search and query syntax ([#291](https://github.com/fooyin/fooyin/pull/291))
* VU and peak meter plugin ([#317](https://github.com/fooyin/fooyin/pull/317))

### Improvements

* Engine
  - Support playback of DSD ([#325](https://github.com/fooyin/fooyin/pull/325))
  - Only convert to 64bit float when necessary
* Interface
  - Add mnemonics to all menubar actions 
  - Improve volume tooltip positioning ([#328](https://github.com/fooyin/fooyin/pull/328))
  - Reduce amount of filesystem queries for tracks without artwork
* Scripting
  - Add $split for splitting fields with a delimiter
  - Add $elide_mid, $elide_end ([#316](https://github.com/fooyin/fooyin/pull/316))
* Search
  - Add different search modes to unconnected search widgets
  - Add settings page under Library for configuration
  - Remove minimum character limit and show message when empty ([#307](https://github.com/fooyin/fooyin/issues/307))
* CUE: Support reading ReplayGain
* DirBrowser: Rename elide text setting to 'Show horizontal scrollbar'
* LibraryTree: Add ability to display artwork
* Properties: Improve appearance
* Settings: Save state-related settings to XDG_STATE_HOME ([#312](https://github.com/fooyin/fooyin/issues/312))

### Fixes

* Engine
  - Resolve rare crash when resuming playback on startup
  - Fix gapless playback occasionally failing on some outputs
* Playlist
  - Fix stop after current behaviour ([#302](https://github.com/fooyin/fooyin/issues/302))
  - Fix restoring state in rare instances
  - Improve performance when dropping a large number of tracks
* Search
  - Fix crash when dragging tracks from dialog
  - Fix incorrect track count
* CMake: Fix finding SndFile ([#305](https://github.com/fooyin/fooyin/issues/305))
* Controls: Fix playlist controls not responding to external changes
* Library: Fix race condition leading to a repeated scan request
* ReplayGain: Fix editing of values
* Scripting: Fix $iflonger behaviour ([#310](https://github.com/fooyin/fooyin/issues/310))
* TreeItem: Fix undefined behaviour


## [0.7.3](https://github.com/fooyin/fooyin/releases/tag/v0.7.3) (2024-09-28)

### Improvements

* Decoding: Support vgm7z
* Properties: Improve appearance

### Fixes

* Decoding: Resolve crash when restarting playback of some decoders
* Layout: Resolve crash when leaving layout editing mode

## [0.7.2](https://github.com/fooyin/fooyin/releases/tag/v0.7.2) (2024-09-26)

### Improvements

* General
  - Add option to use 'Various Artists' for compilations ([#286](https://github.com/fooyin/fooyin/issues/286))
  - Don't exit immediately on File->Quit if fading is enabled
* DirBrowser
  - Add option to toggle text elision ([#285](https://github.com/fooyin/fooyin/issues/285))
  - Add shortcut support ([#292](https://github.com/fooyin/fooyin/issues/292))
  - Add tooltips for elided text
* Search
  - Add dialog for searching entire library
  - Match all terms (words) individually
  - Update window title and loading text based on search mode
* CLI: Add playback control options ([#287](https://github.com/fooyin/fooyin/issues/287))
* Engine: Support playback of ASF
* FFmpeg: Add option to enable all supported formats
* Interface: Add shortcuts for adjusting volume
* Playlist: Add repeat album, shuffle albums, and random playback modes ([#122](https://github.com/fooyin/fooyin/pull/122))
* Scripting: Add $rand ([#295](https://github.com/fooyin/fooyin/pull/295))
* Scrobbler: Add toggle button widget

### Fixes

* Settings
  - Fix saving/restoring decoder and tag reader order
  - Fix manual editing of spinboxes on some pages
* Engine: Resolve crash when attempting to restart playback with disabled decoder
* ExpandedTreeView: Fix empty space at top/bottom in right caption mode
* DirBrowser: Fix expand/collapse in tree mode
* FFmpeg: Resolve crash on failing to open input
* M3UParser: Handle Windows filepaths ([#289](https://github.com/fooyin/fooyin/issues/289))
* Scripting: Fix $meta and $info calls
* SeekBar: Fix setting width on startup
* Selection Info: Fix formatting of durations longer than 1 day ([#290](https://github.com/fooyin/fooyin/pull/290))


## [0.7.1](https://github.com/fooyin/fooyin/releases/tag/v0.7.1) (2024-09-21)

### Improvements

* Interface: Add option to lock splitters ([#280](https://github.com/fooyin/fooyin/issues/280))
* OpenMpt: Add config dialog
* ScriptEditor: Open with currently selected track if available

### Fixes

* Interface: Fix a few misspellings
* ScriptFormatter: Fix parsing of '<' in metadata fields ([#279](https://github.com/fooyin/fooyin/issues/279))
* SelectionInfo: Fix empty model on some systems ([#281](https://github.com/fooyin/fooyin/issues/281))
* TagEditor: Fix changing rating


## [0.7.0](https://github.com/fooyin/fooyin/releases/tag/v0.7.0) (2024-09-19)

### New Features

* ReplayGain support ([#251](https://github.com/fooyin/fooyin/pull/251), [#262](https://github.com/fooyin/fooyin/pull/262))
  - Adds support for reading and applying RG info
* Scrobbling
  - Last.fm and ListenBrainz are currently supported

### Improvements

* General
  - Support multiple values for composer, performer
  - Store additional fields: codec profile, encoding, tool, tag types
* Interface
  - FontButton: Apply set font to button text
  - Show dynamic bitrate in status bar for vbr/abr tracks ([#250](https://github.com/fooyin/fooyin/issues/250))
  - ToolTip: Improve positioning
  - Volume: Add slider dB tooltip ([#267](https://github.com/fooyin/fooyin/pull/267))
* Scripting
  - Improve implementation of $abbr and $replace ([#253](https://github.com/fooyin/fooyin/issues/253))
  - Add $ascii function ([#253](https://github.com/fooyin/fooyin/issues/253))
* SelectionInfo 
  - Add optional ReplayGain section
  - Hide empty entries 
* Engine: Handle audio samples as float64 ([#265](https://github.com/fooyin/fooyin/pull/265))
* FileOps: Replace directory separators in variable calls ([#253](https://github.com/fooyin/fooyin/issues/253))
* PlaylistOrganiser: Support dropping tracks on existing playlists
* TagEditor: Support customising default fields ([#256](https://github.com/fooyin/fooyin/issues/256))

### Fixes

* Interface
  - Resolve crash when changing layouts
  - Fix fonts not being set correctly on startup
  - Fix painting alternating row colours
  - Fix toggling selection in some views
  - LogSlider: Fix undefined behaviour
  - SeekBar: Fix some fonts causing widget to resize ([#277](https://github.com/fooyin/fooyin/issues/277))
* Engine
  - Fix playback not pausing while muted ([#258](https://github.com/fooyin/fooyin/pull/258))
  - Fix listened duration carrying over to next track
  - Fix track always being considered played when restoring state on startup
* Filters:
  - Fix incorrect item width on startup
  - Fix page up/down in artwork mode
* PlaylistOrganiser
  - Resolve crash when deleting active playlist
  - Fix shortcuts not working after right-click
* General: Fix building with Qt6.8
* Library: Notify of updated tracks while reloading
* Scripting: Support split values with $swapprefix, $stripprefix
* Selection Info: Fix sorting of entries
* WaveBar: Fix restoring playing/paused state


## [0.6.2](https://github.com/fooyin/fooyin/releases/tag/v0.6.2) (2024-08-31)

### New Features

* Theme support
  - Ability to change palette colours and fonts
  - Includes dark mode theme
  - Option to save to layouts on export

### Improvements

* Filters: Enable library viewer playlist by default
* Scripting: Make variable calls case-insensitive
* Settings: Add page to toggle and adjust order of decoders/tag readers
* VolumeControl: Support enabling both icon and slider

### Fixes

* Controls: Fix volume slider resizing splitter ([#248](https://github.com/fooyin/fooyin/issues/248))
* Engine: Fix crash when reading short archive entries
* ExpandedTreeView: Fix setting uniform row heights
* Playlist
  - Fix crash when switching to single-column mode
  - Fix track repeating with shuffle + repeat playlist enabled ([#245](https://github.com/fooyin/fooyin/issues/245))
  - Fix some rare instances of duplicate tracks
  - Fix auto-naming of new playlists for tracks with limited metadata 
* Search: Fix sorting by column


## [0.6.1](https://github.com/fooyin/fooyin/releases/tag/v0.6.1) (2024-08-27)

### Improvements

* Playlist: Add file menu option to save all playlists
* UI: Improve titles of menu entries which will open dialogs

### Fixes

* Playlist: Fix extensions not being automatically added when saving playlists
* WaveBar: Fix memory leak when generating waveforms


## [0.6.0](https://github.com/fooyin/fooyin/releases/tag/v0.6.0) (2024-08-27)

## New Features

### Plugins

* Archive support
  - Ability to add and play music directly from archives
* File operations
  - Options to rename, copy, and move files on disk
* Audio Inputs:
  - SndFile - based on libsndfile
  - VGMInput - based on libvgm
  - OpenMPT
  - Game Music Emu
  - RawAudio (bin files)

### Playlist
- Auto-export playlists to a custom location ([#211](https://github.com/fooyin/fooyin/issues/211))
- Option to stop playback at a specified track ([#219](https://github.com/fooyin/fooyin/issues/219))
- Search functionality ([#109](https://github.com/fooyin/fooyin/issues/109), [#199](https://github.com/fooyin/fooyin/issues/199))
- Rating column with inline editing support

### Others
- Log dialog with adjustable log level
- Shortcut support for layouts
- Support for alphanumeric track numbers ([#209](https://github.com/fooyin/fooyin/issues/209))

### Improvements

* Engine
  - Resample audio if output doesn't support format ([#200](https://github.com/fooyin/fooyin/issues/200), [#201](https://github.com/fooyin/fooyin/issues/201))
  - Delay exiting until track has faded out
  - Clamp fade durations ([#222](https://github.com/fooyin/fooyin/pull/222))
  - Use S-curve for longer fade durations ([#237](https://github.com/fooyin/fooyin/pull/237))
  - ALSA: Make buffer, period size configurable
* Library
  - Support reading/writing playcount from files
  - Make writing rating, playcount to files optional
  - Add option to rescan a track selection from disk
  - Match extensions case-insensitively
  - Store codec as a string
  - Add separate sort script for incoming/external files
  - Add options for marking unavailable tracks ([#195](https://github.com/fooyin/fooyin/issues/195))
  - Add options to restrict and exclude file types
* Playlist
  - Support %playingicon% in any column ([#184](https://github.com/fooyin/fooyin/issues/184))
  - Add undo/redo to sorting ([#191](https://github.com/fooyin/fooyin/issues/191))
* Plugins
  - Display plugins list as a tree
  - Add about dialog to all plugins
  - Add optional configure dialog
* Scripting
  - Add %directory%, %firstplayed%, %lastplayed%, %libraryname%, %librarypath%
* Selection Info
  - Add options to prefer showing playing or selected tracks ([#181](https://github.com/fooyin/fooyin/issues/181), [#182](https://github.com/fooyin/fooyin/issues/182))
  - Add option to show extended metadata ([#210](https://github.com/fooyin/fooyin/issues/210))
  - Improve appearance on track selection change
* Settings:
  - Display play action as Play/Pause on shortcut settings page ([#197](https://github.com/fooyin/fooyin/issues/197))
  - Use space key as the default shortcut for play/pause ([#197](https://github.com/fooyin/fooyin/issues/197))
  - Add shortcut to remove current playlist ([#198](https://github.com/fooyin/fooyin/issues/198))
  - Improve display order of settings pages
  - Move several groups of settings to more suitable pages
  - Add action to open script editor for script inputs
* TagEditor
  - Add copy/paste
  - Add auto track number tool
  - Improve display of values for multiple tracks
  - Support direct editing of rating
* UI
  - Add a dedicated layout menubar entry
  - Add option to change application style
  - Add option to show status tips in the status bar
  - Add option to change seek step
  - Artwork Panel: Support changing alignment ([#232](https://github.com/fooyin/fooyin/pull/232))
  - Status Bar: Show message if in layout editing mode
  - Add action to open script in script editor for most setting pages
  - Add library menu option to optimise database
  - Add icon for script editor dialog
* WaveBar
  - Increase number of updates when generating waveform
  - Start playback when seeking in stopped state
* LibraryTree: Add option to open folder in folder structure view ([#175](https://github.com/fooyin/fooyin/issues/175))
* Playback: Add setting to control played threshold
* PlaylistTabs: Add playlist content submenu ([#188](https://github.com/fooyin/fooyin/issues/188))

### Fixes

* Engine
  - Fix playback of APE files ([#173](https://github.com/fooyin/fooyin/issues/173))
  - Fix playback of AAC files
  - Fix playback of tracks without a duration ([#183](https://github.com/fooyin/fooyin/issues/183))
  - Fix restoration of last played state on some systems ([#206](https://github.com/fooyin/fooyin/issues/206))
  - Fix pausing with hardware devices
  - Fix track ending immediately on restarting playback
  - Resolve rare crash when seeking in very short files
  - Handle seeking while fading ([#215](https://github.com/fooyin/fooyin/issues/215))
  - Fix seeking near end of track ([#218](https://github.com/fooyin/fooyin/issues/218))
  - Always reinit decoder when track was stopped ([#221](https://github.com/fooyin/fooyin/pull/221))
  - Fix crash on trying to play non-existent file
* Filters
  - Fix empty filters on startup
  - Resolve crashes when adding/updating tracks
  - Fix artwork mode captions with hidden sections
* Library
  - Fix regression with adding subdirectories ([#174](https://github.com/fooyin/fooyin/issues/174))
  - Fix multi-value fields not being stored ([#179](https://github.com/fooyin/fooyin/issues/179))
  - Fix adding files + dirs in same dir ([#197](https://github.com/fooyin/fooyin/issues/197))
  - Resolve occasional crashes when sorting tracks
  - Resolve crashes when rescanning tracks
  - Fix rescans of existing cue tracks
  - Fix setting added time, modified time
  - Resolve rare crash on exit in middle of library scan
  - Handle '/' separator for artists in id3v2.3 ([#216](https://github.com/fooyin/fooyin/issues/216))
* LibraryTree
  - Fix display of split field values ([#179](https://github.com/fooyin/fooyin/issues/179))
  - Fix adding new tracks ([#187](https://github.com/fooyin/fooyin/issues/187))
* PipeWire
  - Resolve crashes for some audio formats ([#183](https://github.com/fooyin/fooyin/issues/183), [#185](https://github.com/fooyin/fooyin/issues/185))
  - Fix silent playback after opening settings
* Playlist
  - Fix incorrect order of tracks when loading playlists
  - Fix duplicate track entries when dropping on new playlist ([#177](https://github.com/fooyin/fooyin/issues/177))
  - Fix parsing of some cue formats ([#178](https://github.com/fooyin/fooyin/issues/178), [#224](https://github.com/fooyin/fooyin/issues/224))
  - Fix shuffle skipping tracks ([#180](https://github.com/fooyin/fooyin/issues/180))
  - Resolve crash when reading single track cues
  - Resolve crash when adding new columns
  - Fix middle click actions
  - Fix parsing of non-UTF-8 files ([#223](https://github.com/fooyin/fooyin/issues/223))
* TagEditor
  - Fix display of identical field values
  - Fix display of custom tags
  - Fix duplicate tracks on updating metadata
  - Fix sorting new values
  - Fix editing metadata for some file types
  - Fix removing list values ([#179](https://github.com/fooyin/fooyin/issues/179))
  - Fix writing common fields ([#179](https://github.com/fooyin/fooyin/issues/179))
  - Fix duplicate rating fields on read after write
  - Fix editor becoming readonly after right-click
  - Fix setting max rating
  - Fix display of multiple numeric field values ([#214](https://github.com/fooyin/fooyin/issues/214))
* AutoHeaderView: Fix issues updating width for last section
* CoverWidget: Consider DPI when scaling image ([#242](https://github.com/fooyin/fooyin/issues/242))
* DirBrowser: Fix direct playback
* Layout: Fix window size always being saved
* PlaylistTabs: Switch playlist when scrolling over playlist tabs ([#240](https://github.com/fooyin/fooyin/issues/240))
* QueueViewer: Resolve memory leak from unparented actions
* Selection Info: Fix display of multiple album artists ([#216](https://github.com/fooyin/fooyin/issues/216))
* Settings: Fix changing language on restart ([#176](https://github.com/fooyin/fooyin/issues/176))
* WaveBar: Don't draw cursor when stopped

## Packaging

* Add libvgm as an optional git submodule
* Add libsndfile, libopenmpt, libgme and libarchive as optional dependencies


## [0.5.3](https://github.com/fooyin/fooyin/releases/tag/v0.5.3) (2024-07-07)

### Fixes

* LibraryScanner
  - Fix reading file properties for external files
  - Fix reading duration for cue tracks
* Playlist: Fix start playback on send to new playlist failing


## [0.5.2](https://github.com/fooyin/fooyin/releases/tag/v0.5.2) (2024-07-07)

### New Features

* Playback queue viewer/editor

### Improvements

* Playlist
  - Add cut, copy, paste, and crop actions ([#156](https://github.com/fooyin/fooyin/issues/156))
  - Find a common field to use as playlist name when sending to a new playlist
  - Add options to control behaviour when opening files externally
  - Add middle-click options to add to playback queue
  - Improve performance of large playlists
* Playlist Tabs
  - Add optional clear button ([#114](https://github.com/fooyin/fooyin/issues/114))
  - Support dropping tracks to create a playlist
  - Support dropping external files
  - Set expand setting to off by default
* Layout editing mode:
  - Give splitters a more descriptive and accurate name ([#161](https://github.com/fooyin/fooyin/issues/161))
  - Don't show parent splitter menu when adding widgets to a new splitter ([#162](https://github.com/fooyin/fooyin/issues/162))
* Library Filter
  - Add right labels option
  - Improve artwork mode margins
  - Improve scrolling behaviour
* Library Tree
  - Go up on left key press ([#169](https://github.com/fooyin/fooyin/issues/169))
  - Trigger double-click on enter/return key press
* Search
  - Add filepath to search fields
  - Only start searching at 2 characters
* Artwork: Add responsive thumbnails
* Tab Stack: Use dialog for editing tab names when set to east or west position
* Translations: Dutch added ([#168](https://github.com/fooyin/fooyin/issues/168))

### Fixes

* Playlist
  - Fix select all not selecting all for large playlists
  - Fix clear not clearing large playlists
  - Fix an instance of header state not bring restored ([#166](https://github.com/fooyin/fooyin/issues/166))
* Library Filter
  - Fix unscrollable view
  - Fix empty view with no library
* CoverProvider: Fix crashes when attempting to read directory artwork ([#163](https://github.com/fooyin/fooyin/issues/163))
* Engine: Fix crash when changing device in an error state
* Playlist Tabs: Resolve crash when dropping tracks
* Scripting: Fix crashes using pad functions ([#167](https://github.com/fooyin/fooyin/issues/167))
* Selection Info: Avoid showing invalid values


## [0.5.1](https://github.com/fooyin/fooyin/releases/tag/v0.5.1) (2024-06-29)

### Improvements

* Playback
  - Add option to silently continue playback if track unavailable
  - Show message box for ALSA device errors
* Library Tree: Make animation of expand/collapse configurable
* Volume Control: Add slider mode

### Fixes

* Playlist
  - Fix unscrollable view when changing playlist
  - Fix selection playlists refreshing during library scans
* Engine: Fix silence when resuming from a stopped state with fading enabled
* ExpandedTreeView: Stop autoscrolling to selection if partly visible
* Filters: Fix restoring selection on track update
* Library Tree: Fix text colour for playing row
* Scripting: Fix usage of custom tags


## [0.5.0](https://github.com/fooyin/fooyin/releases/tag/v0.5.0) (2024-06-28)

### New Features

* CUE sheet support (including embedded) ([#136](https://github.com/fooyin/fooyin/pull/136))
* Import/export playlists (m3u/m3u8)
* Library Filter: Add artwork mode ([#149](https://github.com/fooyin/fooyin/pull/149))
* Add support for tag parser and audio decoder plugins

### Improvements

* Interface
  - Remember last open directory in file dialogs
  - Make window title playing script configurable
  - Add separate icon for repeat track mode ([#131](https://github.com/fooyin/fooyin/issues/131))
  - Improve appearance of dark icon theme
  - Add layout switching menu to view menu ([#150](https://github.com/fooyin/fooyin/issues/150))
  - Add option to save window size when exporting layouts ([#150](https://github.com/fooyin/fooyin/issues/150))
* Playlist Tabs
  - Add option to expand to fill
  - Use middle-click to create/delete playlists ([#147](https://github.com/fooyin/fooyin/issues/147))
* Library Tree: Add direct playback support ([#148](https://github.com/fooyin/fooyin/issues/148))
* Playlist Organiser: Support dropping tracks ([#146](https://github.com/fooyin/fooyin/issues/146))
* Tag Editor: Improve editing of metadata
* Tagging: Add ability to set ratings via context menu or shortcuts ([#152](https://github.com/fooyin/fooyin/issues/152))
* Widgets: Add playlist switcher widget

### Fixes

* Library
  - Fix crash when aborting library scans
  - Fix scan progress dialog not closing
* Selection Info
  - Fix several display issues
  - Improve performance
* Playlist
  - Fix flickering scrollbar when resizing ([#141](https://github.com/fooyin/fooyin/issues/141))
  - Improve restoration of state
  - Resolve crashes when removing multiple rows of different headers
* MPRIS
  - Fix artwork not updating for different tracks
  - Fix incorrect colour of icons when changing playmode 
* Engine: Fix crash when stopping playback without a valid track
* Track: Fix channel count being incorrectly displayed as 0


## [0.4.5](https://github.com/fooyin/fooyin/releases/tag/v0.4.5) (2024-06-05)

### Fixes

* Fix playback on startup if current output not found
* Fix pause not pausing playback position ([#133](https://github.com/fooyin/fooyin/issues/133))


## [0.4.4](https://github.com/fooyin/fooyin/releases/tag/v0.4.4) (2024-06-04)

### Improvements

* General
  - Add optional system tray icon ([#93](https://github.com/fooyin/fooyin/issues/93))
  - Save playlists, settings at regular intervals ([#127](https://github.com/fooyin/fooyin/issues/127))
* Playlist
  - Add bit depth/bits per sample column
  - Support configuring selection playlist keep alive behaviour
  - Add view option to show currently playing track
* Scripting:
  - Add playback variables ([#104](https://github.com/fooyin/fooyin/issues/104))
  - Add additional string functions ([#104](https://github.com/fooyin/fooyin/issues/104))
  - Add fallbacks for some fields if empty ([#130](https://github.com/fooyin/fooyin/issues/130))
* Selection Info
  - Support hiding/showing sections
  - Remember scroll position
  - Add additional technical fields ([#116](https://github.com/fooyin/fooyin/issues/116))
* ALSA: Support selecting hardware devices ([#119](https://github.com/fooyin/fooyin/issues/119))
* Engine: Add fading options for pause/stop ([#123](https://github.com/fooyin/fooyin/issues/123))
* Library Tree: Add option to remember expanded state
* Tag Editor: Add support for rating tags ([#115](https://github.com/fooyin/fooyin/issues/115))
* WaveBar: Add option to change number of samples used for waveform data
* Widgets: Add lyrics widget ([#118](https://github.com/fooyin/fooyin/pull/118))

### Fixes

* Playlist
  - Fix scaling of artwork ([#120](https://github.com/fooyin/fooyin/issues/120))
  - Fix keyboard controls for scrolling to top/bottom
  - Preserve previous sort order when sorting by column ([#126](https://github.com/fooyin/fooyin/issues/126))
  - Resolve crash when updating non-contiguous rows ([#127](https://github.com/fooyin/fooyin/issues/127))
* Engine: Handle device loss ([#110](https://github.com/fooyin/fooyin/issues/110))
* Interface: Fix scaling of some tooltips

  
## [0.4.3](https://github.com/fooyin/fooyin/releases/tag/v0.4.3) (2024-05-23)

### Improvements

* Interface
  - Support changing volume by scrolling on column icon ([#106](https://github.com/fooyin/fooyin/pull/106))
  - Add options to control appearance of tool buttons
  - Add options to override margins, splitter handle sizes
  - Make quick setup dialog modal
* Playlist
  - Add channels column
  - Add last modified column
  - Add follow playback options to playback menu
  - Improve performance when updating playback statistics ([#103](https://github.com/fooyin/fooyin/issues/103))
* WaveBar
  - Add optional labels
  - Improve behaviour when seeking

### Fixes

* General
  - Fix loading system translations
  - Fix some adding libraries while layout editing mode is active ([#90](https://github.com/fooyin/fooyin/issues/90))
* Interface
  - Fix font/row height inconsistencies ([#101](https://github.com/fooyin/fooyin/issues/101))
  - Fix layout editing mode not working under recent Qt versions ([#111](https://github.com/fooyin/fooyin/issues/111))
  - Fix wrong album art being displayed for untagged/poorly tagged files
  - Resolve crashes using undo/redo in layout editing mode
* Playlist
  - Fix autoscrolling when updating tracks
  - Fix dropping at end of playlist
  - Fix loading default column alignments
  - Fix sorting track numbers under some systems ([#112](https://github.com/fooyin/fooyin/issues/112))
  - Handle column name/script changes
  - Resolve rare crash on startup
* MPRIS
  - Fix artwork caching ([#87](https://github.com/fooyin/fooyin/issues/87))
  - Fix sending firstUsed, lastUsed metadata
* PipeWire
  - Fix playback of some file types ([#100](https://github.com/fooyin/fooyin/issues/100))
  - Fix playback of files with more than 3 channels
  - Handle server restarts ([#85](https://github.com/fooyin/fooyin/issues/85))
* Engine: Fix high-res file playback ([#100](https://github.com/fooyin/fooyin/issues/100))
* DirBrowser: Fix hang when using double click to play
* LibraryTree: Fix track count decrementing on tracks updating
* TagEditor: Fix saving changed metadata
* WaveBar: Fix crash when seeking ([#105](https://github.com/fooyin/fooyin/issues/105))


## [0.4.2](https://github.com/fooyin/fooyin/releases/tag/v0.4.2) (2024-05-16)

### New Features

* Playlist: Artwork columns

### Improvements

* Artwork
  - Add configurable pixmap cache size
  - Update cache for changed tracks
  - Load image data asynchronously ([#75](https://github.com/fooyin/fooyin/pull/75))
  - Add svg icon for the no cover/placeholder image
* Playlist
  - Add option to start playback on send
  - Add option to resume previous playback state on startup
  - Improve add/remove and hide/show of columns
  - Improve performance when removing tracks
  - Improve scaling of artwork
* PlaylistTabs
  - Add option to show 'add playlist' button ([#83](https://github.com/fooyin/fooyin/issues/83))
  - Add status icon to active playlist
* WaveBar
  - Add context options to control track waveform data
  - Add current disk cache size, with option to clear
* Layouts: Add setting to control root margin
* PlaylistOrganiser: Add status icon, background to active playlist
* Settings: Improve layout/formatting of some pages
* Scripting: Add playlist depth variable, functions to pad string left/right
* Sorting: Use QCollator for natural sorting ([#73](https://github.com/fooyin/fooyin/pull/73))

### Fixes

* Playlist
  - Fix crash when active playlist is empty
  - Fix crash when removing rows
  - Fix crash when using undo/redo
  - Fix display issues switching to single-column mode
  - Fix index for keep active playlist
  - Fix incorrect playing index
  - Fix playback on toggling 'playback follows cursor'
  - Fix removing moved columns
  - Fix select all not selecting all
* DirBrowser: Fix double-click, middle-click actions
* Engine: Fix playback of high resolution audio ([#76](https://github.com/fooyin/fooyin/issues/76))
* InfoWidget: Fix crash for languages other than English
* Library: Fix updating monitoring status
* LibraryTree: Fix double-click behaviour
* PlaylistOrganiser: Fix selecting current playlist on startup
* TagEditor: Fix updating album artist field


## [0.4.1](https://github.com/fooyin/fooyin/releases/tag/v0.4.1) (2024-04-14)

### New Features

* Artwork: Directory artwork discovery ([#68](https://github.com/fooyin/fooyin/issues/68))
* Artwork: Support reading front cover, back cover, artist
* Artwork: Add option to prefer playing track or selection
* Artwork: Add option to keep aspect ratio

### Improvements

* Playlist
  - Add bitrate, samplerate columns
  - Show queue indexes in single-column mode
* DirBrowser
  - Add mouse button support
  - Restore selected row when changing directory
* Artwork: Improve performance when resizing
* Layouts: Improve file structure
* Layout editing mode: Improve insertion of widgets; add widget splitting ([#71](https://github.com/fooyin/fooyin/issues/71))
* Player: Support restarting playback after stopping ([#69](https://github.com/fooyin/fooyin/issues/69))

### Fixes

* Playlist
  - Fix empty extension column
  - Fix auto scrolling under Wayland
* Layouts: Fix exporting ([#67](https://github.com/fooyin/fooyin/issues/67))
* PipeWire: Fix playback issues ([#70](https://github.com/fooyin/fooyin/issues/70))
* PlaylistTabs: Fix restoring playlist at startup
* SeekBar: Fix tooltip display issues
* WaveBar: Fix seek issues with some styles


## [0.4.0](https://github.com/fooyin/fooyin/releases/tag/v0.4.0) (2024-04-06)

### Features

* DB: Schema migration support ([#61](https://github.com/fooyin/fooyin/pull/61))
* Engine: Configurable buffer size
* Playlist: Global playback queue
* Plugins: Add options to disable individual plugins
* Plugins: MPRIS support
* Plugins: Waveform seekbar ([#60](https://github.com/fooyin/fooyin/pull/64), [#52](https://github.com/fooyin/fooyin/issues/52))
* Scripting: Custom metadata support
* Scripting: Formatting support in playlist ([#63](https://github.com/fooyin/fooyin/pull/63))
* SearchWidget: Add option to change placeholder text
* Seekbar: Add options to toggle labels, elapsed total
* Widgets: Directory browser ([#58](https://github.com/fooyin/fooyin/pull/58), [#54](https://github.com/fooyin/fooyin/issues/54))

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
* Library: Fix searching for album artwork in file directory ([#59](https://github.com/fooyin/fooyin/issues/59))
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
* SearchWidget: Fix crash using search after changing connections ([#53](https://github.com/fooyin/fooyin/issues/53))
* Seekbar: Respond to external position changes


## [0.3.10](https://github.com/fooyin/fooyin/releases/tag/v0.3.10) (2024-02-10)

### Changes
* General build system improvements

### Fixes
* StatusWidget: Fix flickering when playing a track with an ongoing library scan ([#47](https://github.com/fooyin/fooyin/pull/47))


## [0.3.9](https://github.com/fooyin/fooyin/releases/tag/v0.3.9) (2024-02-10)

### Fixes

* Build system fixes

## [0.3.8](https://github.com/fooyin/fooyin/releases/tag/v0.3.8) (2024-02-10)

### Changes

* General build system improvements
* Add support for building on Ubuntu 22.04 ([#49](https://github.com/fooyin/fooyin/pull/49))
* Filters: Disable filter selection playlist by default
* PlaylistOrganiser: Clear selection when changing current playlist

### Fixes

* LibraryTree: Fix duplicate tracks when switching layouts ([#44](https://github.com/fooyin/fooyin/issues/44))
* Library: Correctly sort tracks with large track numbers ([#46](https://github.com/fooyin/fooyin/issues/46))
* PlaylistOrganiser: Resolve crash when removing all playlists ([#48](https://github.com/fooyin/fooyin/issues/48))
* PlaylistWidget: Restore state on initialisation to keep playing icon active ([#50](https://github.com/fooyin/fooyin/issues/50))
* PlaylistWidget: Reset state/history when using send to new/current playlist
* PlaylistTabs: Correctly restore index to current playlist on initialisation
* Translations: Return correct translation path
* Track: Fix passing track-related containers in queued signals
* ScriptSandbox: Fix setting default state
* AudioDecoder: Fix crashes when decoding files in quick succession


## [0.3.7](https://github.com/fooyin/fooyin/releases/tag/v0.3.7) (2024-02-05)

### Features

* Add support for changing language (English and Chinese for now) ([#40](https://github.com/fooyin/fooyin/pull/40))

### Fixes

* Fix reading audio properties using older TagLib versions ([#41](https://github.com/fooyin/fooyin/issues/41))
* Fix removing custom tags from mp4 tags
* Fix library and track scan requests not running
* Improve ability to find installed FFmpeg libraries
* Fix high CPU usage when playing tracks (~10% -> ~1%)
* Fix playlist not switching when a playlist is removed
* Fix occasional crash when removing tracks from a playlist

## [0.3.6](https://github.com/fooyin/fooyin/releases/tag/v0.3.6) (2024-02-02)

### Changes

* Implement AudioDecoder as a separate, self-contained object to handle all file decoding ([#39](https://github.com/fooyin/fooyin/pull/39))
* Improve plugin naming scheme (fyplugin_name)

### Fixes

* Make creation and passing of AudioOutput to AudioEngine thread-safe
* Add additional safety checks to pipewire output


## [0.3.5](https://github.com/fooyin/fooyin/releases/tag/v0.3.5) (2024-02-01)

### Fixes

* Fix function call in AudioRenderer


## [0.3.4](https://github.com/fooyin/fooyin/releases/tag/v0.3.4) (2024-01-31)

### Changes

* Remove HoverMenu, LogSlider, and MenuHeader from public API

### Fixes

* Fix crashes using PipeWire output

### Packaging

* Further improve plugin CMake setup
* Correctly pass vars to CMakeMacros for plugin development
* Install license, readme to data dir
* Add a CMake uninstall target
* Overhaul build instructions; see [BUILD.md](https://github.com/fooyin/fooyin/blob/master/BUILD.md)


## [0.3.3](https://github.com/fooyin/fooyin/releases/tag/v0.3.3) (2024-01-31)

### Changes

* Simplify plugin CMake setup

### Fixes

* Fix plugin/library rpath issues


## [0.3.2](https://github.com/fooyin/fooyin/releases/tag/v0.3.2) (2024-01-29)

### Changes

* Move AudioEngine to public API
* Improve AudioBuffer implementation
* Support TagLib 2.0 ([#38](https://github.com/fooyin/fooyin/issues/38))


## [0.3.1](https://github.com/fooyin/fooyin/releases/tag/v0.3.1) (2024-01-26)

### Changes

* Rewrite library scan request handling
* Return a ScanRequest for MusicLibrary::rescan

### Fixes

* Only report library scan progress on stopping thread if we're actually running
* Remove leftover debug message


## [0.3.0](https://github.com/fooyin/fooyin/releases/tag/v0.3.0) (2024-01-26)

### Features

* Command line support
* Support opening files/directories with fooyin

### Changes

* Create AudioBuffer, AudioFormat to handle audio data ([#37](https://github.com/fooyin/fooyin/pull/37))
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


## [0.2.1](https://github.com/fooyin/fooyin/releases/tag/v0.2.0) (2024-01-23)

* Fix crash when a library scan detects new and changed tracks


## [0.2.0](https://github.com/fooyin/fooyin/releases/tag/v0.2.0) (2024-01-23)

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


## [0.1.0](https://github.com/fooyin/fooyin/releases/tag/v0.1.0) (2024-01-21)

* Initial release of fooyin.
