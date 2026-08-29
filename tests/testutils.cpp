/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "testutils.h"

#include <core/corepaths.h>
#include <core/coresettings.h>
#include <core/engine/input/playcounttagpolicy.h>
#include <core/engine/input/ratingtagpolicy.h>
#include <core/playlist/playlist.h>
#include <utils/fileutils.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace Fooyin::Testing {
namespace {
QString ratingSettingsPath()
{
    static const QTemporaryDir configDir{QDir::tempPath() + QStringLiteral("/fooyin-rating-test-XXXXXX")};
    EXPECT_TRUE(configDir.isValid());

    QStandardPaths::setTestModeEnabled(true);
    qputenv("XDG_CONFIG_HOME", configDir.path().toUtf8());
    return Core::settingsPath();
}
} // namespace

QString testFilePath(const QString& relativePath)
{
    const QDir testsDir{QStringLiteral(FOOYIN_TEST_SOURCE_DIR)};
    return testsDir.absoluteFilePath(relativePath);
}

void resetRatingSettings()
{
    const QString settingsPath = ratingSettingsPath();
    if(QFileInfo::exists(settingsPath)) {
        ASSERT_TRUE(QFile::remove(settingsPath));
    }
    ASSERT_TRUE(QDir{}.mkpath(QFileInfo{settingsPath}.absolutePath()));
    QSettings settings{settingsPath, QSettings::IniFormat};
    settings.remove(RatingSettings::ReadTag);
    settings.remove(RatingSettings::ReadScale);
    settings.remove(RatingSettings::WriteTag);
    settings.remove(RatingSettings::WriteScale);
    settings.remove(RatingSettings::ReadId3Popm);
    settings.remove(RatingSettings::WriteId3Popm);
    settings.remove(RatingSettings::PopmOwner);
    settings.remove(RatingSettings::PopmMapping);
    settings.remove(RatingSettings::ReadAsfSharedRating);
    settings.remove(RatingSettings::WriteAsfSharedRating);
    settings.remove(PlaycountSettings::ReadTag);
    settings.remove(PlaycountSettings::WriteTag);
    settings.remove(PlaycountSettings::ReadId3Popm);
    settings.remove(PlaycountSettings::WriteId3Popm);
    settings.sync();
}

TempResource::TempResource(const QString& filename, QObject* parent)
    : QFile{parent}
    , m_tempDir{QDir::tempPath() + QStringLiteral("/fooyin-test-XXXXXX")}
    , m_file{filename}
{
    QString resourceName = QStringLiteral("resource");
    const QString suffix = QFileInfo{filename}.suffix();
    if(!suffix.isEmpty()) {
        resourceName += u'.' + suffix;
    }
    setFileName(m_tempDir.filePath(resourceName));

    if(m_tempDir.isValid() && open(QIODevice::ReadWrite)) {
        QFile resource{filename};
        if(resource.open(QIODevice::ReadOnly)) {
            write(resource.readAll());
            flush();
        }
    }

    seek(0);
}

TempResource::~TempResource()
{
    close();
}

QString TempResource::fileName()
{
    // TagLib opens paths independently, which requires releasing our handle on Windows
    flush();
    close();
    return QFile::fileName();
}

bool TempResource::seek(qint64 position)
{
    if(!isOpen() && !open(QIODevice::ReadWrite)) {
        return false;
    }
    return QFile::seek(position);
}

void TempResource::checkValid() const
{
    QByteArray origFileData;
    QByteArray tmpFileData;
    {
        QFile origFile{m_file};
        const bool isOpen = origFile.open(QIODevice::ReadOnly);

        EXPECT_TRUE(origFile.isOpen());

        if(isOpen) {
            origFileData = origFile.readAll();
            origFile.close();
        }
    }

    {
        QFile tmpFile{QFile::fileName()};
        const bool isOpen = tmpFile.open(QIODevice::ReadOnly);

        EXPECT_TRUE(tmpFile.isOpen());

        if(isOpen) {
            tmpFileData = tmpFile.readAll();
            tmpFile.close();
        }
    }

    EXPECT_TRUE(!origFileData.isEmpty());
    EXPECT_TRUE(!tmpFileData.isEmpty());
    EXPECT_EQ(origFileData, tmpFileData);
}

std::unique_ptr<Playlist> PlaylistTestUtils::createPlaylist(const QString& name, SettingsManager* settings)
{
    return Playlist::create(name, settings);
}

void PlaylistTestUtils::replaceTracks(Playlist& playlist, const TrackList& tracks)
{
    playlist.replaceTracks(tracks);
}

void PlaylistTestUtils::changeCurrentIndex(Playlist& playlist, int index)
{
    playlist.changeCurrentIndex(index);
}

void StubMusicLibrary::setTracks(TrackList tracks)
{
    m_tracks = std::move(tracks);
    m_libraryTracks.reset();
}

void StubMusicLibrary::setLibraryTracks(TrackList tracks)
{
    m_libraryTracks = std::move(tracks);
}

void StubMusicLibrary::setLibrary(LibraryInfo library)
{
    m_library = std::move(library);
}

void StubMusicLibrary::emitTracksLoaded()
{
    Q_EMIT tracksLoaded(m_tracks);
}

void StubMusicLibrary::emitTracksUpdatedForTests(const TrackList& tracks)
{
    for(const Track& updatedTrack : tracks) {
        const auto it = std::ranges::find_if(m_tracks, [&updatedTrack](const Track& libraryTrack) {
            return updatedTrack.id() >= 0 ? libraryTrack.id() == updatedTrack.id()
                                          : libraryTrack.sameIdentityAs(updatedTrack);
        });

        if(it != m_tracks.end()) {
            *it = updatedTrack;
        }
    }

    Q_EMIT tracksUpdated(tracks);
}

bool StubMusicLibrary::hasLibrary() const
{
    return m_library.has_value();
}

std::optional<LibraryInfo> StubMusicLibrary::libraryInfo(int id) const
{
    return m_library && m_library->id == id ? m_library : std::nullopt;
}

std::optional<LibraryInfo> StubMusicLibrary::libraryForPath(const QString& path) const
{
    if(m_library && (Utils::File::isSamePath(path, m_library->path) || Utils::File::isSubdir(path, m_library->path))) {
        return m_library;
    }
    return std::nullopt;
}

void StubMusicLibrary::loadAllTracks() { }

bool StubMusicLibrary::isEmpty() const
{
    return m_tracks.empty();
}

void StubMusicLibrary::refreshAll() { }

void StubMusicLibrary::rescanAll() { }

ScanRequest StubMusicLibrary::refresh(const LibraryInfo& /*library*/)
{
    return {.type = ScanRequest::Library, .cancel = []() { }};
}

ScanRequest StubMusicLibrary::rescan(const LibraryInfo& /*library*/)
{
    return {.type = ScanRequest::Library, .cancel = []() { }};
}

void StubMusicLibrary::cancelScan(int /*id*/) { }

ScanRequest StubMusicLibrary::scanTracks(const TrackList& /*tracks*/)
{
    return {.type = ScanRequest::Tracks, .cancel = []() { }};
}

ScanRequest StubMusicLibrary::scanModifiedTracks(const TrackList& /*tracks*/)
{
    return {.type = ScanRequest::Tracks, .cancel = []() { }};
}

ScanRequest StubMusicLibrary::scanFiles(const QList<QUrl>& /*files*/)
{
    return {.type = ScanRequest::Files, .cancel = []() { }};
}

ScanRequest StubMusicLibrary::loadPlaylist(const QList<QUrl>& /*files*/)
{
    return {.type = ScanRequest::Playlist, .cancel = []() { }};
}

TrackList StubMusicLibrary::tracks() const
{
    return m_tracks;
}

TrackList StubMusicLibrary::libraryTracks() const
{
    return m_libraryTracks.value_or(m_tracks);
}

Track StubMusicLibrary::trackForId(int id) const
{
    const auto it = std::ranges::find_if(m_tracks, [id](const Track& track) { return track.id() == id; });
    return it != m_tracks.cend() ? *it : Track{};
}

TrackList StubMusicLibrary::tracksForIds(const TrackIds& ids) const
{
    TrackList tracks;
    tracks.reserve(ids.size());
    for(const int id : ids) {
        if(const Track track = trackForId(id); track.isValid()) {
            tracks.emplace_back(track);
        }
    }
    return tracks;
}

std::shared_ptr<TrackMetadataStore> StubMusicLibrary::metadataStore() const
{
    return {};
}

void StubMusicLibrary::updateTrack(const Track& /*track*/) { }

void StubMusicLibrary::updateTracks(const TrackList& /*tracks*/) { }

void StubMusicLibrary::updateTrackMetadata(const TrackList& /*tracks*/) { }

WriteRequest StubMusicLibrary::writeTrackMetadata(const TrackList& /*tracks*/)
{
    return {};
}

WriteRequest StubMusicLibrary::writeTrackCovers(const TrackCoverData& /*coverData*/)
{
    return {};
}

PendingTrackCoverProvider* StubMusicLibrary::pendingTrackCoverProvider() const
{
    return nullptr;
}

void StubMusicLibrary::updateTrackStats(const TrackList& /*tracks*/, Track::Stats /*stats*/) { }

void StubMusicLibrary::updateTrackStats(const Track& /*track*/, Track::Stats /*stats*/) { }

WriteRequest StubMusicLibrary::removeUnavailbleTracks()
{
    return {};
}

WriteRequest StubMusicLibrary::deleteTracks(const TrackList& /*tracks*/)
{
    return {};
}
} // namespace Fooyin::Testing
