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

#include <core/engine/audioloader.h>
#include <core/library/musiclibrary.h>
#include <core/network/remoteioservice.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlistloader.h>
#include <core/trackmetadatastore.h>

#include <QDateTime>
#include <QLoggingCategory>
#include <QPointer>

#include <iterator>
#include <ranges>

Q_DECLARE_LOGGING_CATEGORY(LIB_SCANNER)

using namespace Qt::StringLiterals;

namespace {
bool trackListUsesEmbeddedCue(const Fooyin::TrackList& tracks)
{
    return std::ranges::any_of(tracks, [](const Fooyin::Track& track) { return track.hasExtraTag(u"CUESHEET"_s); });
}

bool anyTrackWasExplicitlyDropped(const Fooyin::TrackList& tracks, const std::set<QString>& explicitPaths)
{
    return std::ranges::any_of(tracks, [&explicitPaths](const Fooyin::Track& track) {
        return explicitPaths.contains(Fooyin::normalisePath(track.filepath()));
    });
}

bool pathIsInDroppedDirectory(const QString& path, const std::set<QString>& explicitDirs)
{
    return std::ranges::any_of(explicitDirs,
                               [&path](const auto& dir) { return path == dir || path.startsWith(dir + u"/"_s); });
}

bool anyTrackWasCoveredByDroppedDirectory(const Fooyin::TrackList& tracks, const std::set<QString>& explicitDirs)
{
    return std::ranges::any_of(tracks, [&explicitDirs](const Fooyin::Track& track) {
        return pathIsInDroppedDirectory(Fooyin::normalisePath(track.filepath()), explicitDirs);
    });
}

std::set<QString> physicalPathsForTracks(const Fooyin::TrackList& tracks)
{
    std::set<QString> paths;
    for(const auto& track : tracks) {
        paths.emplace(Fooyin::normalisePath(track.filepath()));
    }
    return paths;
}
} // namespace

namespace Fooyin {
class LibraryScanSession::PlaylistScanOperation
{
public:
    PlaylistScanOperation(LibraryScanSession* session, QList<QUrl> urls, QObject* context,
                          std::function<void(bool, TrackList)> callback)
        : m_session{session}
        , m_urls{std::move(urls)}
        , m_context{context}
        , m_callback{std::move(callback)}
        , m_resolver{std::make_unique<LibraryTrackResolver>(session->makeResolver())}
        , m_deliveryContext{std::make_unique<QObject>()}
    { }

    ~PlaylistScanOperation()
    {
        m_stopCallback.reset();
        m_deliveryContext.reset();
        if(m_download) {
            m_download->cancel();
        }
    }

    void start(std::stop_token token)
    {
        m_stopCallback = std::make_unique<StopCallback>(token, [this] {
            QMetaObject::invokeMethod(m_deliveryContext.get(), [this] { complete(false); }, Qt::QueuedConnection);
        });
        QMetaObject::invokeMethod(m_deliveryContext.get(), [this] { advance(); }, Qt::QueuedConnection);
    }

private:
    using StopCallback = std::stop_callback<std::function<void()>>;

    void advance()
    {
        while(m_session->m_state.mayRun() && m_index < m_urls.size()) {
            const QUrl url = m_urls.at(m_index++);
            if(Track::isRemotePath(url.toString())) {
                m_session->m_state.reportProgress(url.toString());
                m_download = m_resolver->readPlaylistAsync(
                    url, m_deliveryContext.get(), [this](bool completed, TrackList tracks) {
                        QMetaObject::invokeMethod(
                            m_deliveryContext.get(),
                            [this, completed, tracks = std::move(tracks)]() mutable {
                                m_download.reset();
                                if(!completed || !m_session->m_state.mayRun()) {
                                    complete(false);
                                    return;
                                }
                                m_result.insert(m_result.end(), std::make_move_iterator(tracks.begin()),
                                                std::make_move_iterator(tracks.end()));
                                advance();
                            },
                            Qt::QueuedConnection);
                    });
                return;
            }

            const auto count = m_resolver->countPlaylistTracks(url);
            m_session->m_state.setProgressPhase(m_session->m_phase, m_session->m_state.progressCount() + count);
            m_session->m_state.reportProgress(url.toString());

            auto tracks = m_resolver->readPlaylist(url);
            m_result.insert(m_result.end(), std::make_move_iterator(tracks.begin()),
                            std::make_move_iterator(tracks.end()));
        }

        complete(m_session->flushWriter(true));
    }

    void complete(bool completed)
    {
        if(std::exchange(m_finished, true)) {
            return;
        }

        m_stopCallback.reset();
        // Invalidate queued work before releasing the resolver or aborting the reply
        m_deliveryContext.reset();
        auto download = std::move(m_download);
        if(download) {
            download->cancel();
        }
        m_resolver.reset();
        m_urls.clear();

        auto callback = std::move(m_callback);
        auto tracks   = std::move(m_result);
        if(m_context) {
            QMetaObject::invokeMethod(
                m_context,
                [callback = std::move(callback), completed, tracks = std::move(tracks)]() mutable {
                    callback(completed, std::move(tracks));
                },
                Qt::QueuedConnection);
        }
    }

    LibraryScanSession* m_session;
    QList<QUrl> m_urls;
    QPointer<QObject> m_context;
    std::function<void(bool, TrackList)> m_callback;
    std::unique_ptr<LibraryTrackResolver> m_resolver;
    std::unique_ptr<QObject> m_deliveryContext;
    std::shared_ptr<RemoteDownloadHandle> m_download;
    TrackList m_result;
    qsizetype m_index{0};
    bool m_finished{false};
    std::unique_ptr<StopCallback> m_stopCallback;
};

LibraryScanSession::LibraryScanSession(TrackDatabase* trackDatabase, PlaylistLoader* playlistLoader,
                                       AudioLoader* audioLoader, std::shared_ptr<RemoteIoService> remoteIo,
                                       std::shared_ptr<TrackMetadataStore> metadataStore, LibraryScanConfig config,
                                       LibraryScanHost* host)
    : m_host{host}
    , m_config{std::move(config)}
    , m_trackDatabase{trackDatabase}
    , m_playlistLoader{playlistLoader}
    , m_audioLoader{audioLoader}
    , m_remoteIo{std::move(remoteIo)}
    , m_metadataStore{std::move(metadataStore)}
    , m_state{host}
    , m_writer{trackDatabase, [this](const ScanResult& result) { handleScanWriterFlush(result); }}
    , m_resolver{nullptr}
    , m_fileScanResult{nullptr}
    , m_onlyModified{true}
    , m_enumerationMode{EnumerationMode::Library}
    , m_phase{ScanProgress::Phase::ReadingMetadata}
{ }

LibraryScanSession::LibraryScanSession(TrackDatabase* trackDatabase, PlaylistLoader* playlistLoader,
                                       AudioLoader* audioLoader, std::shared_ptr<TrackMetadataStore> metadataStore,
                                       LibraryScanConfig config, LibraryScanHost* host)
    : LibraryScanSession{
          trackDatabase, playlistLoader, audioLoader, nullptr, std::move(metadataStore), std::move(config), host}
{ }

LibraryScanSession::~LibraryScanSession() = default;

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
        finaliseUnseenTracks();
        flushWriter(true);
    }

    return completed && m_state.mayRun();
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
        finaliseUnseenTracks();
        flushWriter(true);
    }

    return completed && m_state.mayRun();
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

    m_externalExplicitPaths.clear();
    m_externalExplicitDirs.clear();
    m_externalCueCoveredPaths.clear();

    for(const auto& path : std::as_const(paths)) {
        const QString normalisedPath = normalisePath(path);
        m_externalExplicitPaths.emplace(normalisedPath);
        if(QFileInfo{path}.isDir()) {
            m_externalExplicitDirs.emplace(normalisedPath);
        }
    }

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

    m_externalExplicitPaths.clear();
    m_externalExplicitDirs.clear();
    m_externalCueCoveredPaths.clear();

    return completed && m_state.mayRun();
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

        totalEntries += resolver.countPlaylistTracks(url);
    }

    m_state.setProgressPhase(m_phase, totalEntries);
    m_state.reportProgress({});

    for(const auto& url : urls) {
        if(!m_state.mayRun()) {
            return false;
        }

        const TrackList playlistTracks = resolver.readPlaylist(url);
        result.insert(result.end(), playlistTracks.cbegin(), playlistTracks.cend());
    }

    return flushWriter(true);
}

void LibraryScanSession::scanPlaylistAsync(const TrackList& libraryTracks, const QList<QUrl>& urls, QObject* context,
                                           std::function<void(bool completed, TrackList tracks)> callback,
                                           std::stop_token token)
{
    reset({});

    m_state.populateExistingTracks(libraryTracks, {}, false);
    m_state.setReportEnumeratingProgress(true);
    m_phase = ScanProgress::Phase::ReadingMetadata;

    m_playlistScan = std::make_unique<PlaylistScanOperation>(this, urls, context, std::move(callback));
    m_playlistScan->start(token);
}

void LibraryScanSession::finish()
{
    m_phase = ScanProgress::Phase::Finished;
    m_state.setProgressPhase(m_phase, m_state.progressCount());
    m_state.reportProgress({});
}

void LibraryScanSession::fail(QString error)
{
    m_state.fail(std::move(error));
}

QString LibraryScanSession::error() const
{
    return m_state.error();
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

void LibraryScanSession::reset(const LibraryInfo& library)
{
    m_playlistScan.reset();

    m_currentLibrary = library;
    m_state.reset();
    m_writer.reset();
    m_resolver       = nullptr;
    m_fileScanResult = nullptr;
    m_externalExplicitPaths.clear();
    m_externalExplicitDirs.clear();
    m_externalCueCoveredPaths.clear();
    m_onlyModified    = true;
    m_enumerationMode = EnumerationMode::Library;
}

bool LibraryScanSession::flushWriter(const bool finalFlush)
{
    if(!m_state.mayRun()) {
        return false;
    }
    if(m_writer.empty()) {
        return true;
    }

    if(finalFlush) {
        m_state.setProgressPhase(ScanProgress::Phase::WritingDatabase, 0);
        m_state.reportProgress({});
    }

    if(auto result = m_writer.flush(); !result) {
        fail(std::move(result.error()));
        return false;
    }
    return true;
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

    const auto flushWrites = [this]() {
        flushTrackResolverWrites();
    };

    return LibraryTrackResolver{m_currentLibrary,
                                m_playlistLoader,
                                m_audioLoader,
                                m_config.playlistSkipMissing,
                                m_remoteIo,
                                m_metadataStore,
                                m_trackDatabase,
                                &m_state,
                                &m_writer,
                                {.overwriteRatingOnReload    = m_config.overwriteRatingOnReload,
                                 .overwritePlaycountOnReload = m_config.overwritePlaycountOnReload},
                                flushWrites};
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
            if(m_config.addFoldersIgnorePlaylists && !m_externalExplicitPaths.contains(filepath)) {
                m_state.rememberScannedFile(filepath);
                return true;
            }

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
            if(type == EnumeratedFileType::Track && m_externalCueCoveredPaths.contains(filepath)) {
                m_state.fileScanned(filepath);
                return true;
            }

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
                if(trackListUsesEmbeddedCue(cueTracks)
                   && (anyTrackWasExplicitlyDropped(cueTracks, m_externalExplicitPaths)
                       || anyTrackWasCoveredByDroppedDirectory(cueTracks, m_externalExplicitDirs))) {
                    return true;
                }

                const auto coveredPaths = physicalPathsForTracks(cueTracks);
                m_externalCueCoveredPaths.insert(coveredPaths.cbegin(), coveredPaths.cend());

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

void LibraryScanSession::finaliseUnseenTracks()
{
    m_phase = ScanProgress::Phase::Finalising;
    m_state.setProgressPhase(m_phase, 0);
    m_state.reportProgress({});

    for(const auto& track : m_state.scopedTracks()) {
        if(!m_state.mayRun()) {
            return;
        }
        if(m_state.trackWasSeen(track)) {
            continue;
        }

        // Existing tracks are marked seen whenever their source is still valid, including when
        // metadata cannot be refreshed. Anything left unseen after a scan is either
        // gone or no longer accepted by the exclusion filters.

        qCDebug(LIB_SCANNER) << "Track no longer in scan scope:" << track.prettyFilepath();

        Track disabledTrack{track};
        disabledTrack.setLibraryId(-1);
        disabledTrack.setIsEnabled(false);
        m_writer.updateTrack(disabledTrack);
    }
}
} // namespace Fooyin
