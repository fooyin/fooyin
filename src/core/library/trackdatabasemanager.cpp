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

#include "trackdatabasemanager.h"

#include "database/trackdatabase.h"
#include "internalcoresettings.h"
#include "statisticsutils.h"

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <core/engine/input/ratingtagpolicy.h>
#include <core/library/libraryutils.h>
#include <core/library/musiclibrary.h>
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/scopeguard.h>
#include <utils/settings/settingsmanager.h>

#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

#include <map>
#include <ranges>
#include <set>

Q_LOGGING_CATEGORY(TRK_DBMAN, "fy.trackdbmanager")

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
bool shouldContinue(const std::stop_token& stopToken)
{
    return !stopToken.stop_requested();
}

AudioReader::WriteOptions writeOptionsForStats(Track::Stats stats)
{
    AudioReader::WriteOptions options{AudioReader::None};
    if(stats.testFlag(Track::Stat::Rating)) {
        options |= AudioReader::Rating;
    }
    if(stats.testFlag(Track::Stat::Playcount)) {
        options |= AudioReader::Playcount;
    }
    return options;
}

bool syncRawRatingTag(Track& track, const AudioLoader* audioLoader = nullptr, AudioReader::WriteOptions options = {})
{
    const RatingTagPolicy policy = ratingTagPolicy();
    const QString tag            = policy.effectiveWriteTag();

    if(audioLoader) {
        const QDateTime accessTime
            = options.testFlag(AudioReader::PreserveTimestamps) ? QFileInfo{track.filepath()}.lastRead() : QDateTime{};

        Track writtenTrack{track.filepath(), track.subsong()};
        const bool read = audioLoader->readTrackMetadata(writtenTrack);
        if(accessTime.isValid()) {
            QFile file{track.filepath()};
            if(file.open(QIODevice::ReadOnly)) {
                file.setFileTime(accessTime, QFileDevice::FileAccessTime);
            }
        }
        if(!read) {
            return false;
        }

        bool changed{false};
        for(const auto& name : {u"RATING"_s, u"FMPS_RATING"_s, u"POPM"_s, tag}) {
            if(name.isEmpty()) {
                continue;
            }
            QString value = writtenTrack.rawRatingTag(name);
            if(value.isEmpty()) {
                value = writtenTrack.extraTag(name).value(0);
            }
            changed |= track.rawRatingTag(name) != value;
            track.setRawRatingTag(name, value);
            if(track.hasExtraTag(name)) {
                track.replaceExtraTag(name, writtenTrack.extraTag(name));
            }
        }
        return changed;
    }

    if(tag.isEmpty()) {
        return false;
    }

    const QString value = formatTextRating(track.rating(), policy.writeScale);
    if(track.rawRatingTag(tag) == value) {
        return false;
    }

    track.setRawRatingTag(tag, value);
    return true;
}
} // namespace

TrackDatabaseManager::TrackDatabaseManager(DbConnectionPoolPtr dbPool, std::shared_ptr<AudioLoader> audioLoader,
                                           SettingsManager* settings, std::shared_ptr<TrackMetadataStore> metadataStore,
                                           QObject* parent)
    : Worker{parent}
    , m_dbPool{std::move(dbPool)}
    , m_audioLoader{std::move(audioLoader)}
    , m_settings{settings}
    , m_metadataStore{std::move(metadataStore)}
{ }

void TrackDatabaseManager::initialiseThread()
{
    Worker::initialiseThread();

    m_dbHandler = std::make_unique<DbConnectionHandler>(m_dbPool);
    m_trackDatabase.initialise(DbConnectionProvider{m_dbPool});
    m_trackDatabase.setMetadataStore(m_metadataStore);
}

void TrackDatabaseManager::getAllTracks(std::stop_token workerToken)
{
    if(!startWork(workerToken)) {
        return;
    }

    const TrackList tracks = m_trackDatabase.getAllTracks();
    Q_EMIT gotTracks(tracks);

    setState(Idle);
}

void TrackDatabaseManager::checkTrackAvailability(const TrackList& tracks, std::stop_token workerToken)
{
    if(!startWork(workerToken)) {
        return;
    }

    TrackList updatedTracks;
    updatedTracks.reserve(tracks.size());

    for(const Track& track : tracks) {
        if(!mayRun()) {
            break;
        }

        const bool enabled = track.exists();
        if(track.isEnabled() == enabled) {
            continue;
        }

        Track updatedTrack{track};
        updatedTrack.setIsEnabled(enabled);
        updatedTracks.push_back(updatedTrack);
    }

    if(!updatedTracks.empty()) {
        Q_EMIT availabilityChecked(updatedTracks);
    }

    setState(Idle);
}

void TrackDatabaseManager::updateTracks(const TrackList& tracks, bool write, std::stop_token workerToken)
{
    updateTracks(tracks, write, -1, {}, workerToken);
}

void TrackDatabaseManager::updateTracks(const TrackList& tracks, bool write, int operationId, std::stop_token stopToken,
                                        std::stop_token workerToken, std::optional<Track::Stats> fileTagStats,
                                        bool statisticsCommitted)
{
    if(!startWork(workerToken)) {
        Q_EMIT updatedTracks({});
        if(operationId >= 0) {
            Q_EMIT trackWriteCompleted(operationId, {}, 0, true);
        }
        return;
    }

    TrackList tracksToUpdate{tracks};
    TrackList tracksUpdated;
    std::map<int, Track> writtenTracks;
    int failedCount{0};
    bool cancelled{false};

    AudioReader::WriteOptions options;

    if(write) {
        options |= AudioReader::Metadata;
        if(fileTagStats ? fileTagStats->testFlag(Track::Stat::Rating)
                        : m_settings->value<Settings::Core::SaveRatingToMetadata>()) {
            options |= AudioReader::Rating;
        }
        if(fileTagStats ? fileTagStats->testFlag(Track::Stat::Playcount)
                        : m_settings->value<Settings::Core::SavePlaycountToMetadata>()) {
            options |= AudioReader::Playcount;
        }
        if(m_settings->value<Settings::Core::PreserveTimestamps>()) {
            options |= AudioReader::PreserveTimestamps;
        }
    }

    for(const Track& track : std::as_const(tracksToUpdate)) {
        if(!shouldContinue(stopToken) || !mayRun()) {
            cancelled = true;
            break;
        }

        Track updatedTrack{track};
        if(statisticsCommitted) {
            const auto current
                = PlaybackStatisticsDatabase{DbConnectionProvider{m_dbPool}}.trackStatistics(updatedTrack.id());
            if(!current) {
                ++failedCount;
                continue;
            }
            updatedTrack.replaceStatistics(*current, Track::Stat::All);
            updatedTrack.setAddedTime(current->addedTime());
            updatedTrack.setStatisticsKey(current->statisticsKey());
            updatedTrack.setStatisticsRevision(current->statisticsRevision());
        }
        else if(!m_trackDatabase.refreshStaleStatistics(updatedTrack)) {
            ++failedCount;
            continue;
        }

        if(write && !updatedTrack.isDatabaseOnlyMetadata()) {
            if(m_audioLoader->writeTrackMetadata(updatedTrack, options)) {
                if(options.testFlag(AudioReader::Rating)) {
                    syncRawRatingTag(updatedTrack, m_audioLoader.get(), options);
                }
                const QDateTime modifiedTime = QFileInfo{updatedTrack.filepath()}.lastModified();
                updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
                updatedTrack.normaliseExtraProperties();
                writtenTracks.insert_or_assign(updatedTrack.id(), updatedTrack);
            }
            else {
                qCWarning(TRK_DBMAN) << "Failed to write metadata to file:" << updatedTrack.filepath();
                ++failedCount;
                continue;
            }
        }

        if(updatedTrack.hash().isEmpty()) {
            updatedTrack.generateHash();
        }

        tracksUpdated.push_back(std::move(updatedTrack));
    }

    auto committed = m_trackDatabase.updateTrackMetadata(std::move(tracksUpdated));
    failedCount += committed.failed;
    tracksUpdated = std::move(committed.tracks);

    Track::Stats enabledTagStats;
    if(options.testFlag(AudioReader::Rating)) {
        enabledTagStats |= Track::Stat::Rating;
    }
    if(options.testFlag(AudioReader::Playcount)) {
        enabledTagStats |= Track::Stat::Playcount;
    }

    std::set<int> failedStatisticsWrites;
    for(auto& track : tracksUpdated) {
        const auto written = writtenTracks.find(track.id());
        if(written == writtenTracks.cend()) {
            continue;
        }

        Track::Stats changedStats;
        if(track.playCount() != written->second.playCount()) {
            changedStats |= Track::Stat::Playcount;
        }
        if(track.rating() != written->second.rating()) {
            changedStats |= Track::Stat::Rating;
        }
        changedStats &= enabledTagStats;
        if(changedStats == Track::Stat::None) {
            continue;
        }

        const auto corrected = writeTrackStats({.tracks              = {track},
                                                .stats               = changedStats,
                                                .writeToFiles        = true,
                                                .fileTagStats        = enabledTagStats,
                                                .statisticsCommitted = true});
        if(!corrected.tracks.empty()) {
            track = corrected.tracks.front();
        }
        if(corrected.failedTagWrites > 0 || corrected.tracks.empty()) {
            ++failedCount;
            failedStatisticsWrites.insert(track.id());
        }
    }

    Q_EMIT updatedTracks(tracksUpdated);
    if(operationId >= 0) {
        std::erase_if(tracksUpdated, [&failedStatisticsWrites](const Track& track) {
            return failedStatisticsWrites.contains(track.id());
        });
        Q_EMIT trackWriteCompleted(operationId, tracksUpdated, failedCount, cancelled);
    }

    setState(Idle);
}

void TrackDatabaseManager::updateTrackStats(const TrackList& tracks, Track::Stats updatedStats, bool writeToFiles,
                                            std::stop_token workerToken)
{
    updateTrackStats({{.tracks = tracks, .stats = updatedStats, .writeToFiles = writeToFiles}}, workerToken);
}

void TrackDatabaseManager::updateTrackStats(const std::vector<TrackStatsBatch>& batches, std::stop_token workerToken)
{
    if(!startWork(workerToken)) {
        return;
    }

    TrackList tracksUpdated;
    Track::Stats updatedStats;

    for(const auto& batch : batches) {
        if(!mayRun()) {
            break;
        }

        auto result = writeTrackStats(batch);
        if(!result.tracks.empty()) {
            updatedStats |= batch.stats;
            std::ranges::move(result.tracks, std::back_inserter(tracksUpdated));
        }
    }

    if(!tracksUpdated.empty()) {
        std::set<QString> loadedKeys;
        for(const auto& track : tracksUpdated) {
            loadedKeys.emplace(track.statisticsKey());
        }

        std::map<int, Track> sharedTracks;
        for(const auto& shared : m_trackDatabase.tracksByStatisticsKeys(loadedKeys)) {
            sharedTracks.insert_or_assign(shared.id(), shared);
        }

        for(const auto& updated : tracksUpdated) {
            const auto shared = sharedTracks.find(updated.id());
            if(shared != sharedTracks.end()) {
                Track overlay{updated};
                overlay.replaceStatistics(shared->second, Track::Stat::All);
                shared->second = overlay;
            }
        }

        tracksUpdated.clear();

        for(auto& track : sharedTracks | std::views::values) {
            tracksUpdated.push_back(std::move(track));
        }

        Q_EMIT updatedTracksStats(tracksUpdated, updatedStats);
    }

    setState(Idle);
}

PlaybackStatisticsDatabase::MaintenanceResult
TrackDatabaseManager::manageStatistics(const PlaybackStatisticsRequest& request, const QString& activeSourceKey,
                                       std::stop_token workerToken, QPromise<PlaybackStatisticsResult>* promise)
{
    if(!startWork(workerToken)) {
        PlaybackStatisticsDatabase::MaintenanceResult update;
        update.result.error = tr("Playback statistics maintenance was cancelled.");
        return update;
    }

    const auto resetState = scopeGuard([this]() { setState(Idle); });

    auto update   = m_trackDatabase.manageStatistics(request, promise);
    auto& result  = update.result;
    auto& changed = update.tracks;
    if(!result.error.isEmpty() || changed.empty()) {
        return update;
    }

    Track::Stats enabledTagStats;
    if(request.fileTagStats ? request.fileTagStats->testFlag(Track::Stat::Rating)
                            : m_settings->value<Settings::Core::SaveRatingToMetadata>()) {
        enabledTagStats |= Track::Stat::Rating;
    }
    if(request.fileTagStats ? request.fileTagStats->testFlag(Track::Stat::Playcount)
                            : m_settings->value<Settings::Core::SavePlaycountToMetadata>()) {
        enabledTagStats |= Track::Stat::Playcount;
    }

    std::vector<Track*> tagTracks;
    for(auto& track : changed) {
        if(track.isInArchive() || track.isDatabaseOnlyMetadata()) {
            continue;
        }

        const auto fields = update.fileTagChanges.find(track.id());
        if(fields == update.fileTagChanges.cend() || (fields->second & enabledTagStats) == Track::Stat::None) {
            continue;
        }

        const auto source = Utils::physicalSourceKey(track);
        if(!source || *source != activeSourceKey) {
            tagTracks.push_back(&track);
        }
    }

    StatisticsProgress progress{promise};
    if(!tagTracks.empty()) {
        progress.start(tr("Writing file tags…"), static_cast<int>(tagTracks.size()));
    }
    int processed{0};
    for(auto* track : tagTracks) {
        const auto written = writeTrackStats({.tracks              = {*track},
                                              .stats               = update.fileTagChanges.at(track->id()),
                                              .writeToFiles        = true,
                                              .fileTagStats        = request.fileTagStats,
                                              .statisticsCommitted = true});
        result.failedTagWrites += written.failedTagWrites;
        if(!written.tracks.empty()) {
            *track = written.tracks.front();
        }
        progress.update(++processed);
    }

    return update;
}

TrackDatabaseManager::TrackStatsWriteResult TrackDatabaseManager::writeTrackStats(const TrackStatsBatch& batch)
{
    TrackStatsWriteResult result;

    AudioReader::WriteOptions options{AudioReader::None};
    if(batch.fileTagStats ? batch.fileTagStats->testFlag(Track::Stat::Rating)
                          : m_settings->value<Settings::Core::SaveRatingToMetadata>()) {
        options |= AudioReader::Rating;
    }
    if(batch.fileTagStats ? batch.fileTagStats->testFlag(Track::Stat::Playcount)
                          : m_settings->value<Settings::Core::SavePlaycountToMetadata>()) {
        options |= AudioReader::Playcount;
    }

    const AudioReader::WriteOptions requestedWriteOptions = writeOptionsForStats(batch.stats) & options;
    AudioReader::WriteOptions writeOptions = batch.writeToFiles ? requestedWriteOptions : AudioReader::None;
    if(writeOptions != AudioReader::None && m_settings->value<Settings::Core::PreserveTimestamps>()) {
        writeOptions |= AudioReader::PreserveTimestamps;
    }

    for(const Track& track : batch.tracks) {
        if(!mayRun()) {
            break;
        }

        Track updatedTrack{track};
        if(batch.statisticsCommitted) {
            if(!m_trackDatabase.reloadTrack(updatedTrack)) {
                ++result.failedTagWrites;
                continue;
            }
        }
        else if(!m_trackDatabase.refreshStaleStatistics(updatedTrack)) {
            continue;
        }

        bool needsTrackUpdate{false};
        bool ratingWritten{false};

        if(updatedTrack.hash().isEmpty()) {
            updatedTrack.generateHash();
        }

        if(!track.isInArchive() && !updatedTrack.isDatabaseOnlyMetadata() && writeOptions != AudioReader::None) {
            if(m_audioLoader->writeTrackMetadata(updatedTrack, writeOptions)) {
                ratingWritten = writeOptions.testFlag(AudioReader::Rating);
                if(ratingWritten) {
                    syncRawRatingTag(updatedTrack, m_audioLoader.get(), writeOptions);
                }
                const QDateTime modifiedTime   = QFileInfo{updatedTrack.filepath()}.lastModified();
                const uint64_t newModifiedTime = modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0;
                updatedTrack.setModifiedTime(newModifiedTime);
                updatedTrack.normaliseExtraProperties();
                needsTrackUpdate = ratingWritten || newModifiedTime != track.modifiedTime();
            }
            else {
                qCWarning(TRK_DBMAN) << "Failed to write track playback statistics to file:" << updatedTrack.filepath();
                ++result.failedTagWrites;
            }
        }

        if(!batch.statisticsCommitted && batch.writeToFiles && batch.stats.testFlag(Track::Stat::Rating)
           && !ratingWritten) {
            const QString tag = ratingTagPolicy().effectiveWriteTag();
            if(!tag.isEmpty()) {
                const auto storedTag = m_trackDatabase.rawRatingTag(track.id(), tag);
                if(!storedTag) {
                    continue;
                }
                updatedTrack.setRawRatingTag(tag, *storedTag);
            }
        }

        if((!needsTrackUpdate || m_trackDatabase.updateTrackFileProperties(updatedTrack))
           && (batch.statisticsCommitted || m_trackDatabase.updateTrackStats(updatedTrack, batch.stats))) {
            if(!batch.writeToFiles && requestedWriteOptions.testFlag(AudioReader::Rating) && !track.isInArchive()
               && !track.isDatabaseOnlyMetadata()) {
                // Publish the pending value for scripts, but persist the raw tag only after the file write succeeds
                syncRawRatingTag(updatedTrack);
            }
            result.tracks.push_back(updatedTrack);
        }
        else {
            qCWarning(TRK_DBMAN) << "Failed to update track playback statistics:" << updatedTrack.filepath();
        }
    }

    return result;
}

void TrackDatabaseManager::writeCovers(const TrackCoverData& tracks, std::stop_token workerToken)
{
    writeCovers(tracks, -1, {}, workerToken);
}

void TrackDatabaseManager::writeCovers(const TrackCoverData& tracks, int operationId, std::stop_token stopToken,
                                       std::stop_token workerToken)
{
    if(!startWork(workerToken)) {
        Q_EMIT updatedTracks({});
        if(operationId >= 0) {
            Q_EMIT trackCoverWriteCompleted(operationId, {}, 0, true);
        }
        return;
    }

    TrackList tracksToUpdate{tracks.tracks};
    TrackList tracksUpdated;
    int failedCount{0};
    bool cancelled{false};

    AudioReader::WriteOptions options;
    if(m_settings->value<Settings::Core::PreserveTimestamps>()) {
        options |= AudioReader::PreserveTimestamps;
    }

    for(const auto& track : std::as_const(tracksToUpdate)) {
        if(!shouldContinue(stopToken) || !mayRun()) {
            cancelled = true;
            break;
        }

        if(track.isInArchive()) {
            ++failedCount;
            continue;
        }

        Track updatedTrack{track};
        if(m_audioLoader->writeTrackCover(updatedTrack, tracks.coverData, options)) {
            const QDateTime modifiedTime = QFileInfo{updatedTrack.filepath()}.lastModified();
            updatedTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);

            if(m_trackDatabase.updateTrackModifiedTime(updatedTrack)) {
                tracksUpdated.push_back(updatedTrack);
            }
            else {
                ++failedCount;
            }
        }
        else {
            qCWarning(TRK_DBMAN) << "Failed to update track covers:" << updatedTrack.filepath();
            ++failedCount;
        }
    }

    Q_EMIT updatedTracksFileProperties(tracksUpdated);
    if(operationId >= 0) {
        Q_EMIT trackCoverWriteCompleted(operationId, tracksUpdated, failedCount, cancelled);
    }

    setState(Idle);
}

void TrackDatabaseManager::removeUnavailbleTracks(const TrackList& tracks, std::stop_token workerToken)
{
    removeUnavailbleTracks(tracks, -1, {}, workerToken);
}

void TrackDatabaseManager::deleteTracks(const TrackList& tracks, std::stop_token workerToken)
{
    deleteTracks(tracks, -1, {}, workerToken);
}

void TrackDatabaseManager::deleteTracks(const TrackList& tracks, int operationId, std::stop_token stopToken,
                                        std::stop_token workerToken)
{
    if(!startWork(workerToken)) {
        if(operationId >= 0) {
            Q_EMIT tracksDeleted(operationId, {}, 0, true);
        }
        return;
    }

    TrackList tracksToDelete;
    int failedCount{0};
    bool cancelled{false};

    for(const Track& track : tracks) {
        if(!shouldContinue(stopToken) || !mayRun()) {
            cancelled = true;
            break;
        }

        tracksToDelete.push_back(track);
    }

    if(!tracksToDelete.empty() && m_trackDatabase.deleteTracks(tracksToDelete)) {
        Q_EMIT removedTracks(tracksToDelete);
    }
    else if(!tracksToDelete.empty()) {
        failedCount = static_cast<int>(tracksToDelete.size());
    }

    if(operationId >= 0) {
        Q_EMIT tracksDeleted(operationId, tracksToDelete, failedCount, cancelled);
    }

    setState(Idle);
}

void TrackDatabaseManager::removeUnavailbleTracks(const TrackList& tracks, int operationId, std::stop_token stopToken,
                                                  std::stop_token workerToken)
{
    if(!startWork(workerToken)) {
        if(operationId >= 0) {
            Q_EMIT unavailableTracksRemoved(operationId, {}, 0, true);
        }
        return;
    }

    TrackList trackstoRemove;
    int failedCount{0};
    bool cancelled{false};

    for(const Track& track : tracks) {
        if(!shouldContinue(stopToken) || !mayRun()) {
            cancelled = true;
            break;
        }

        if(!QFileInfo::exists(track.isInArchive() ? track.archivePath() : track.filepath())) {
            trackstoRemove.push_back(track);
        }
    }

    if(!trackstoRemove.empty() && m_trackDatabase.deleteTracks(trackstoRemove)) {
        Q_EMIT removedTracks(trackstoRemove);
    }
    else if(!trackstoRemove.empty()) {
        failedCount = static_cast<int>(trackstoRemove.size());
    }

    if(operationId >= 0) {
        Q_EMIT unavailableTracksRemoved(operationId, trackstoRemove, failedCount, cancelled);
    }

    setState(Idle);
}

void TrackDatabaseManager::cleanupTracks(std::stop_token workerToken)
{
    if(!startWork(workerToken)) {
        return;
    }

    m_settings->set<Settings::Core::ActiveTrackId>(-2);
    m_trackDatabase.cleanupTracks();

    setState(Idle);
}
} // namespace Fooyin

#include "moc_trackdatabasemanager.cpp"
