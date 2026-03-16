/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "libraryscansession.h"

#include "database/trackdatabase.h"
#include "libraryfileenumerator.h"
#include "libraryscanner.h"
#include "libraryscanstate.h"
#include "libraryscanutils.h"
#include "libraryscanwriter.h"
#include "librarytrackresolver.h"
#include "playlist/playlistloader.h"

#include <core/engine/audioloader.h>
#include <core/library/musiclibrary.h>
#include <core/playlist/playlist.h>

#include <QDateTime>
#include <QLoggingCategory>

#include <ranges>

Q_DECLARE_LOGGING_CATEGORY(LIB_SCANNER)

using namespace Qt::StringLiterals;

namespace Fooyin {
LibraryScanSession::LibraryScanSession(TrackDatabase* trackDatabase, PlaylistLoader* playlistLoader,
                                       AudioLoader* audioLoader, LibraryScanConfig config, LibraryScanHost* host)
    : m_host{host}
    , m_config{std::move(config)}
    , m_trackDatabase{trackDatabase}
    , m_playlistLoader{playlistLoader}
    , m_audioLoader{audioLoader}
    , m_state{host}
    , m_writer{trackDatabase, [this](const ScanResult& result) { handleScanWriterFlush(result); }}
    , m_resolver{nullptr}
    , m_fileScanResult{nullptr}
    , m_onlyModified{true}
    , m_enumerationMode{EnumerationMode::Library}
    , m_phase{ScanProgress::Phase::ReadingMetadata}
{ }

LibraryScanSession::~LibraryScanSession() = default;

void LibraryScanSession::reset(const LibraryInfo& library)
{
    m_currentLibrary = library;
    m_state.reset();
    m_writer.reset();
    m_resolver        = nullptr;
    m_fileScanResult  = nullptr;
    m_onlyModified    = true;
    m_enumerationMode = EnumerationMode::Library;
}

void LibraryScanSession::flushWriter(const bool finalFlush)
{
    if(m_writer.empty()) {
        return;
    }

    const auto previousPhase = m_phase;
    m_state.setProgressPhase(ScanProgress::Phase::WritingDatabase, 0);
    m_state.reportProgress({});
    m_writer.flush();

    if(!finalFlush) {
        const size_t total = previousPhase == ScanProgress::Phase::Finished ? m_state.filesScannedCount() : 0;
        m_state.setProgressPhase(previousPhase, total);
        m_state.reportProgress({});
    }
}

void LibraryScanSession::maybeFlushWriter()
{
    if(m_writer.shouldFlush()) {
        flushWriter();
    }
}

LibraryTrackResolver LibraryScanSession::makeResolver()
{
    m_phase = ScanProgress::Phase::ReadingMetadata;
    return LibraryTrackResolver{
        m_currentLibrary, m_playlistLoader, m_audioLoader, m_config.playlistSkipMissing,
        m_trackDatabase,  &m_state,         &m_writer,     [this]() { flushTrackResolverWrites(); }};
}

bool LibraryScanSession::handleEnumeratedFile(const QFileInfo& info, const EnumeratedFileType type)
{
    if(!m_state.mayRun()) {
        return false;
    }

    const QString filepath = normalisePath(info.absoluteFilePath());

    m_phase = ScanProgress::Phase::ReadingMetadata;
    m_state.setProgressPhase(m_phase, 0);
    m_state.reportProgress(filepath);

    if(m_enumerationMode == EnumerationMode::External) {
        if(type == EnumeratedFileType::Playlist) {
            const size_t playlistTrackCount = m_resolver->countPlaylistTracks(filepath);
            m_state.setProgressPhase(m_phase, m_state.progressCount() + playlistTrackCount);
            m_state.reportProgress(filepath);

            const TrackList playlistTracks = m_resolver->readPlaylist(filepath);
            m_fileScanResult->playlistTracksScanned.insert(m_fileScanResult->playlistTracksScanned.end(),
                                                           playlistTracks.cbegin(), playlistTracks.cend());
            m_state.rememberScannedFile(filepath);
            return true;
        }

        if(!m_state.filesScanned().contains(filepath)) {
            if(m_state.trackPaths().contains(filepath)) {
                const auto& existingTracks = m_state.trackPaths().at(filepath);
                m_fileScanResult->tracksScanned.insert(m_fileScanResult->tracksScanned.end(), existingTracks.cbegin(),
                                                       existingTracks.cend());
            }
            else if(m_state.existingArchives().contains(filepath)) {
                const auto& existingTracks = m_state.existingArchives().at(filepath);
                m_fileScanResult->tracksScanned.insert(m_fileScanResult->tracksScanned.end(), existingTracks.cbegin(),
                                                       existingTracks.cend());
            }
            else if(type == EnumeratedFileType::Cue) {
                const TrackList cueTracks = m_resolver->readPlaylistTracks(filepath);
                for(auto track : cueTracks) {
                    readFileProperties(track);
                    track.setAddedTime(QDateTime::currentMSecsSinceEpoch());
                    m_fileScanResult->tracksScanned.push_back(track);
                }
            }
            else {
                const TrackList tracks = m_resolver->readTracks(filepath);
                for(Track track : tracks) {
                    readFileProperties(track);
                    track.setAddedTime(QDateTime::currentMSecsSinceEpoch());

                    if(track.hasExtraTag(u"CUESHEET"_s)) {
                        const TrackList cueTracks = m_resolver->readEmbeddedPlaylistTracks(track);
                        std::ranges::copy(cueTracks, std::back_inserter(m_fileScanResult->tracksScanned));
                    }
                    else {
                        m_fileScanResult->tracksScanned.push_back(track);
                    }
                }
            }
        }
    }
    else {
        switch(type) {
            case EnumeratedFileType::Cue:
                m_resolver->readCue(info, m_onlyModified);
                break;
            case EnumeratedFileType::Track:
                m_resolver->readFile(info, m_onlyModified);
                break;
            case EnumeratedFileType::Playlist:
                break;
        }
    }

    m_state.fileScanned(filepath);
    maybeFlushWriter();
    return m_state.mayRun();
}

void LibraryScanSession::handleScanWriterFlush(const ScanResult& result)
{
    m_host->reportScanUpdate(result);
}

void LibraryScanSession::flushTrackResolverWrites()
{
    maybeFlushWriter();
}

void LibraryScanSession::finaliseMissingTracks()
{
    m_phase = ScanProgress::Phase::Finalising;
    m_state.setProgressPhase(m_phase, 0);
    m_state.reportProgress({});

    for(const auto& track : m_state.scopedTracks()) {
        if(m_state.trackWasSeen(track)) {
            continue;
        }

        bool shouldDisable = !m_state.pathExists(physicalTrackPath(track));

        if(!shouldDisable && track.hasCue() && !track.hasEmbeddedCue()) {
            shouldDisable = !m_state.pathExists(track.cuePath());
        }

        if(!shouldDisable) {
            continue;
        }

        qCDebug(LIB_SCANNER) << "Track not found:" << track.prettyFilepath();

        Track disabledTrack{track};
        disabledTrack.setLibraryId(-1);
        disabledTrack.setIsEnabled(false);
        m_writer.updateTrack(disabledTrack);
    }
}

bool LibraryScanSession::scanLibrary(const LibraryInfo& library, const TrackList& tracks, const bool onlyModified)
{
    reset(library);

    QStringList restrictExtensions      = m_config.libraryRestrictExt;
    const QStringList excludeExtensions = m_config.libraryExcludeExt;

    if(restrictExtensions.empty()) {
        restrictExtensions = normaliseExtensions(m_audioLoader->supportedFileExtensions());
    }
    if(!excludeExtensions.contains(u"cue"_s)) {
        restrictExtensions.append(u"cue"_s);
    }

    for(const auto& extension : excludeExtensions) {
        restrictExtensions.removeAll(extension);
    }

    m_state.populateExistingTracks(tracks, {library.path}, true);
    m_state.setReportEnumeratingProgress(!onlyModified);
    m_phase = onlyModified ? ScanProgress::Phase::ReadingMetadata : ScanProgress::Phase::Enumerating;
    m_state.setProgressPhase(m_phase, 0);
    m_state.reportProgress({});

    auto resolver  = makeResolver();
    m_resolver     = &resolver;
    m_onlyModified = onlyModified;

    LibraryFileEnumerator enumerator{&m_state, [this](const QFileInfo& info, const EnumeratedFileType type) {
                                         return handleEnumeratedFile(info, type);
                                     }};

    const bool completed = enumerator.enumerateFiles({library.path}, restrictExtensions, {});
    m_resolver           = nullptr;

    if(completed && m_state.mayRun()) {
        finaliseMissingTracks();
        flushWriter(true);
    }

    return completed;
}

bool LibraryScanSession::scanDirectories(const LibraryInfo& library, const QStringList& dirs, const TrackList& tracks)
{
    reset(library);

    QStringList restrictExtensions      = m_config.libraryRestrictExt;
    const QStringList excludeExtensions = m_config.libraryExcludeExt;

    if(restrictExtensions.empty()) {
        restrictExtensions = normaliseExtensions(m_audioLoader->supportedFileExtensions());
    }
    if(!excludeExtensions.contains(u"cue"_s)) {
        restrictExtensions.append(u"cue"_s);
    }

    for(const auto& extension : excludeExtensions) {
        restrictExtensions.removeAll(extension);
    }

    m_state.populateExistingTracks(tracks, dirs, true);
    m_state.setReportEnumeratingProgress(false);
    m_phase = ScanProgress::Phase::ReadingMetadata;
    m_state.setProgressPhase(m_phase, 0);
    m_state.reportProgress({});

    auto resolver  = makeResolver();
    m_resolver     = &resolver;
    m_onlyModified = true;

    LibraryFileEnumerator enumerator{&m_state, [this](const QFileInfo& info, const EnumeratedFileType type) {
                                         return handleEnumeratedFile(info, type);
                                     }};

    const bool completed = enumerator.enumerateFiles(dirs, restrictExtensions, {});
    m_resolver           = nullptr;

    if(completed && m_state.mayRun()) {
        finaliseMissingTracks();
        flushWriter(true);
    }

    return completed;
}

bool LibraryScanSession::scanFiles(const TrackList& libraryTracks, const QList<QUrl>& urls,
                                   LibraryScanFilesResult& result)
{
    reset({});
    m_state.populateExistingTracks(libraryTracks, {}, false);

    QStringList restrictExtensions      = m_config.externalRestrictExt;
    const QStringList excludeExtensions = m_config.externalExcludeExt;
    QStringList playlistExtensions      = normaliseExtensions(Playlist::supportedPlaylistExtensions());

    if(restrictExtensions.empty()) {
        restrictExtensions = normaliseExtensions(m_audioLoader->supportedFileExtensions());
    }
    if(!excludeExtensions.contains(u"cue"_s)) {
        restrictExtensions.append(u"cue"_s);
    }

    for(const auto& extension : excludeExtensions) {
        restrictExtensions.removeAll(extension);
        playlistExtensions.removeAll(extension);
    }

    QStringList paths;
    std::ranges::transform(urls, std::back_inserter(paths), [](const QUrl& url) { return url.toLocalFile(); });

    m_phase = ScanProgress::Phase::Enumerating;
    m_state.setReportEnumeratingProgress(true);
    m_state.setProgressPhase(m_phase, 0);
    m_state.reportProgress({});

    auto resolver     = makeResolver();
    m_resolver        = &resolver;
    m_fileScanResult  = &result;
    m_enumerationMode = EnumerationMode::External;

    LibraryFileEnumerator enumerator{&m_state, [this](const QFileInfo& info, const EnumeratedFileType type) {
                                         return handleEnumeratedFile(info, type);
                                     }};

    const bool completed = enumerator.enumerateFiles(paths, restrictExtensions, playlistExtensions);

    if(completed && m_state.mayRun()) {
        flushWriter(true);
    }

    m_fileScanResult  = nullptr;
    m_resolver        = nullptr;
    m_enumerationMode = EnumerationMode::Library;
    return completed;
}

bool LibraryScanSession::scanPlaylist(const TrackList& libraryTracks, const QList<QUrl>& urls, TrackList& result)
{
    reset({});
    m_state.populateExistingTracks(libraryTracks, {}, false);
    m_state.setReportEnumeratingProgress(true);
    m_phase = ScanProgress::Phase::ReadingMetadata;

    auto resolver = makeResolver();
    size_t totalEntries{0};
    for(const auto& url : urls) {
        if(!m_state.mayRun()) {
            return false;
        }

        totalEntries += resolver.countPlaylistTracks(url.toLocalFile());
    }

    m_state.setProgressPhase(m_phase, totalEntries);
    m_state.reportProgress({});

    for(const auto& url : urls) {
        if(!m_state.mayRun()) {
            return false;
        }

        const TrackList playlistTracks = resolver.readPlaylist(url.toLocalFile());
        result.insert(result.end(), playlistTracks.cbegin(), playlistTracks.cend());
    }

    if(m_state.mayRun()) {
        flushWriter(true);
    }

    return true;
}

void LibraryScanSession::finish()
{
    m_phase = ScanProgress::Phase::Finished;
    m_state.setProgressPhase(m_phase, m_state.progressCount());
    m_state.reportProgress({});
}

size_t LibraryScanSession::filesScanned() const
{
    return m_state.filesScannedCount();
}

size_t LibraryScanSession::progressCount() const
{
    return m_state.progressCount();
}

size_t LibraryScanSession::discoveredFiles() const
{
    return m_state.discoveredFiles();
}
} // namespace Fooyin
