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

#include "playbackstatisticsdatabase.h"
#include "databasehelpers.h"

#include "library/statisticsutils.h"

#include <utils/database/dbquery.h>
#include <utils/database/dbtransaction.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

#include <ranges>
#include <set>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
float normaliseTrackRating(float rating)
{
    return rating > 0 ? rating : -1.0F;
}

PlaybackStatisticsDatabase::Statistics readStatistics(const DbQuery& query, int offset = 0)
{
    return {.added       = query.value(offset).toULongLong(),
            .firstPlayed = query.value(offset + 1).toULongLong(),
            .lastPlayed  = query.value(offset + 2).toULongLong(),
            .playCount   = query.value(offset + 3).toInt(),
            .rating      = normaliseTrackRating(query.value(offset + 4).toFloat()),
            .loved       = query.value(offset + 5).toBool()};
}

uint64_t readRevision(const QSqlDatabase& db)
{
    DbQuery query{db, u"SELECT Value FROM Settings WHERE Name = 'PlaybackStatisticsRevision'"_s};
    return query.exec() && query.next() ? query.value(0).toULongLong() : 0;
}

PlaybackStatisticsConfig readConfig(const QSqlDatabase& db)
{
    PlaybackStatisticsConfig config;

    DbQuery query{db, u"SELECT Value FROM Settings WHERE Name = 'PlaybackStatistics'"_s};
    if(query.exec() && query.next()) {
        const auto object = QJsonDocument::fromJson(query.value(0).toByteArray()).object();

        config.script    = object.value(u"script"_s).toString();
        config.playcount = static_cast<PlaycountMergePolicy>(std::clamp(object.value(u"playcount"_s).toInt(), 0, 2));
        config.rating    = static_cast<RatingMergePolicy>(std::clamp(object.value(u"rating"_s).toInt(), 0, 2));
        config.loved     = static_cast<LovedMergePolicy>(std::clamp(object.value(u"loved"_s).toInt(), 0, 2));
        config.clearFirstPlayed  = object.value(u"clearFirstPlayed"_s).toBool();
        config.clearLastPlayed   = object.value(u"clearLastPlayed"_s).toBool();
        config.clearSplitHistory = object.value(u"clearSplitHistory"_s).toBool();
    }

    return config;
}

bool writeConfig(const QSqlDatabase& db, const PlaybackStatisticsConfig& config)
{
    const QJsonObject object{{u"script"_s, config.script},
                             {u"playcount"_s, static_cast<int>(config.playcount)},
                             {u"rating"_s, static_cast<int>(config.rating)},
                             {u"loved"_s, static_cast<int>(config.loved)},
                             {u"clearFirstPlayed"_s, config.clearFirstPlayed},
                             {u"clearLastPlayed"_s, config.clearLastPlayed},
                             {u"clearSplitHistory"_s, config.clearSplitHistory}};

    DbQuery query{db, u"INSERT OR REPLACE INTO Settings (Name, Value) VALUES ('PlaybackStatistics', :value)"_s};
    query.bindValue(u":value"_s, QString::fromUtf8(QJsonDocument{object}.toJson(QJsonDocument::Compact)));
    return query.exec();
}
} // namespace

PlaybackStatisticsDatabase::PlaybackStatisticsDatabase(const DbConnectionProvider& dbProvider)
{
    initialise(dbProvider);
    m_config   = readConfig(db());
    m_revision = readRevision(db());
}

const PlaybackStatisticsConfig& PlaybackStatisticsDatabase::config() const
{
    return m_config;
}

uint64_t PlaybackStatisticsDatabase::revision() const
{
    return m_revision;
}

QString PlaybackStatisticsDatabase::keyForTrack(const Track& track) const
{
    return PlaybackStatistics::evaluateKey(m_config, track);
}

void PlaybackStatisticsDatabase::prepareTrack(Track& track) const
{
    track.setStatisticsKey(keyForTrack(track));
    track.setStatisticsRevision(m_revision);
}

Track PlaybackStatisticsDatabase::Statistics::toTrack() const
{
    Track track;
    applyTo(track);
    return track;
}

void PlaybackStatisticsDatabase::Statistics::applyTo(Track& track) const
{
    track.setAddedTime(added);

    Track values;
    values.setFirstPlayed(firstPlayed);
    values.setLastPlayed(lastPlayed);
    values.setPlayCount(playCount);
    values.setRating(rating);
    values.setLoved(loved);

    track.replaceStatistics(values, Track::Stat::All);
}

std::expected<Track, QSqlError> PlaybackStatisticsDatabase::trackStatistics(int trackId) const
{
    DbQuery query{db(), u"SELECT Tracks.StatisticsKey, AddedDate, FirstPlayed, LastPlayed, PlayCount, Rating, Loved "
                        "FROM Tracks LEFT JOIN TrackStats ON Tracks.StatisticsKey = TrackStats.StatisticsKey "
                        "WHERE TrackID = :id"_s};

    query.bindValue(u":id"_s, trackId);

    if(!query.exec()) {
        return std::unexpected{query.lastError()};
    }
    if(!query.next()) {
        if(query.lastError().isValid()) {
            return std::unexpected{query.lastError()};
        }
        return std::unexpected{QSqlError{u"Track no longer exists"_s, {}, QSqlError::StatementError}};
    }

    Track track;
    track.setId(trackId);
    track.setStatisticsKey(query.value(0).toString());
    track.setStatisticsRevision(m_revision);
    readStatistics(query, 1).applyTo(track);
    return track;
}

bool PlaybackStatisticsDatabase::refreshStaleStatistics(Track& track) const
{
    if(track.id() < 0 || track.statisticsRevision() >= m_revision) {
        return true;
    }

    const auto current = trackStatistics(track.id());
    if(!current) {
        return false;
    }

    track.replaceStatistics(*current, Track::Stat::All);
    track.setAddedTime(current->addedTime());
    track.setStatisticsKey(current->statisticsKey());
    track.setStatisticsRevision(m_revision);
    return true;
}

bool PlaybackStatisticsDatabase::refreshStatistics(TrackList& tracks) const
{
    std::set<QString> uniqueKeys;
    for(const auto& track : tracks) {
        uniqueKeys.insert(track.statisticsKey());
    }

    const std::vector<QString> keys{uniqueKeys.begin(), uniqueKeys.end()};
    std::map<QString, Statistics> stored;
    static constexpr size_t BatchSize{500};

    for(int start{0}; std::cmp_less(start, keys.size()); start += BatchSize) {
        const auto end = static_cast<int>(std::min(start + BatchSize, keys.size()));

        QStringList placeholders;
        for(int i{start}; i < end; ++i) {
            placeholders.push_back(u":key%1"_s.arg(i));
        }

        DbQuery query{db(), u"SELECT StatisticsKey, AddedDate, FirstPlayed, LastPlayed, PlayCount, Rating, Loved "
                            "FROM TrackStats WHERE StatisticsKey IN (%1)"_s.arg(placeholders.join(u','))};

        for(int i{start}; i < end; ++i) {
            query.bindValue(placeholders.at(i - start), keys.at(i));
        }
        if(!query.exec()) {
            return false;
        }

        while(query.next()) {
            stored.emplace(query.value(0).toString(), readStatistics(query, 1));
        }
        if(query.lastError().isValid()) {
            return false;
        }
    }

    for(auto& track : tracks) {
        const auto it = stored.find(track.statisticsKey());
        (it == stored.end() ? Statistics{} : it->second).applyTo(track);
        track.setStatisticsRevision(m_revision);
    }

    return true;
}

std::expected<std::optional<PlaybackStatisticsDatabase::Statistics>, QSqlError>
PlaybackStatisticsDatabase::existingStats(const QString& key) const
{
    static const QString statement = u"SELECT AddedDate, FirstPlayed, LastPlayed, PlayCount, Rating, Loved "
                                     "FROM TrackStats WHERE StatisticsKey = :statisticsKey;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":statisticsKey"_s, key);

    if(!query.exec()) {
        return std::unexpected{query.lastError()};
    }

    if(!query.next()) {
        if(query.lastError().isValid()) {
            return std::unexpected{query.lastError()};
        }
        return {};
    }

    return Statistics{
        .added       = query.value(0).toULongLong(),
        .firstPlayed = query.value(1).toULongLong(),
        .lastPlayed  = query.value(2).toULongLong(),
        .playCount   = query.value(3).toInt(),
        .rating      = normaliseTrackRating(query.value(4).toFloat()),
        .loved       = query.value(5).toBool(),
    };
}

std::expected<PlaybackStatisticsDatabase::Statistics, QSqlError>
PlaybackStatisticsDatabase::updateStats(const Track& track, Track::Stats updatedStats) const
{
    const auto current = existingStats(keyForTrack(track));
    if(!current) {
        return std::unexpected{current.error()};
    }
    return updateStats(track, updatedStats, current->value_or(Statistics{}));
}

std::expected<PlaybackStatisticsDatabase::Statistics, QSqlError>
PlaybackStatisticsDatabase::updateStats(const Track& track, Track::Stats updatedStats, const Statistics& current,
                                        bool overwritePlaycount) const
{
    if(track.hash().isEmpty()) {
        return std::unexpected{
            QSqlError{u"Cannot update statistics without a track hash"_s, {}, QSqlError::StatementError}};
    }

    auto [added, firstPlayed, lastPlayed, playCount, rating, loved] = current;
    bool dbNeedsUpdate{false};

    const uint64_t trackAdded       = track.addedTime();
    const uint64_t trackFirstPlayed = track.firstPlayed();
    const uint64_t trackLastPlayed  = track.lastPlayed();
    const int trackPlayCount        = track.playCount();
    const float trackRating         = normaliseTrackRating(track.rating());
    const bool trackLoved           = track.isLoved();

    if((trackAdded != added) && (added == 0 || (trackAdded > 0 && trackAdded < added))) {
        added         = trackAdded;
        dbNeedsUpdate = true;
    }

    if(updatedStats.testFlag(Track::Stat::Playcount)) {
        if((trackFirstPlayed != firstPlayed)
           && (overwritePlaycount || firstPlayed == 0 || (trackFirstPlayed > 0 && trackFirstPlayed < firstPlayed))) {
            firstPlayed   = trackFirstPlayed;
            dbNeedsUpdate = true;
        }
        if((trackLastPlayed != lastPlayed) && (overwritePlaycount || trackLastPlayed > lastPlayed)) {
            lastPlayed    = trackLastPlayed;
            dbNeedsUpdate = true;
        }
        if((trackPlayCount != playCount) && (overwritePlaycount || trackPlayCount > playCount)) {
            playCount     = trackPlayCount;
            dbNeedsUpdate = true;
        }
    }
    if(updatedStats.testFlag(Track::Stat::Rating) && trackRating != rating) {
        rating        = trackRating;
        dbNeedsUpdate = true;
    }
    if(updatedStats.testFlag(Track::Stat::Loved) && trackLoved != loved) {
        loved         = trackLoved;
        dbNeedsUpdate = true;
    }

    if(dbNeedsUpdate) {
        Track replacement;
        replacement.setAddedTime(added);
        replacement.setFirstPlayed(firstPlayed);
        replacement.setLastPlayed(lastPlayed);
        replacement.setPlayCount(playCount);
        replacement.setRating(rating);
        replacement.setLoved(loved);

        if(!replaceStats(keyForTrack(track), replacement)) {
            return std::unexpected{QSqlError{u"Unable to write playback statistics"_s, {}, QSqlError::StatementError}};
        }
    }

    return Statistics{.added       = added,
                      .firstPlayed = firstPlayed,
                      .lastPlayed  = lastPlayed,
                      .playCount   = playCount,
                      .rating      = rating,
                      .loved       = loved};
}

bool PlaybackStatisticsDatabase::replaceStats(const QString& key, const Track& track) const
{
    DbQuery query{db(), u"INSERT OR REPLACE INTO TrackStats "
                        "(StatisticsKey, AddedDate, FirstPlayed, LastPlayed, PlayCount, Rating, Loved) "
                        "VALUES (:key, :added, :first, :last, :count, :rating, :loved)"_s};

    query.bindValue(u":key"_s, key);
    query.bindValue(u":added"_s, QVariant::fromValue(track.addedTime()));
    query.bindValue(u":first"_s, QVariant::fromValue(track.firstPlayed()));
    query.bindValue(u":last"_s, QVariant::fromValue(track.lastPlayed()));
    query.bindValue(u":count"_s, track.playCount());
    query.bindValue(u":rating"_s, track.rating());
    query.bindValue(u":loved"_s, track.isLoved());

    return query.exec();
}

PlaybackStatisticsDatabase::MaintenanceResult
PlaybackStatisticsDatabase::manageStatistics(const PlaybackStatisticsRequest& request,
                                             const std::shared_ptr<TrackMetadataStore>& metadataStore,
                                             QPromise<PlaybackStatisticsResult>* promise)
{
    using Operation = PlaybackStatisticsRequest::Operation;

    MaintenanceResult update;
    auto& result  = update.result;
    result.config = m_config;
    if(request.operation == Operation::Read) {
        return update;
    }

    StatisticsProgress progress{promise};
    progress.start(tr("Reading statistics…"));

    const bool resetting = request.operation == Operation::Reset || request.operation == Operation::PreviewReset;
    const bool preview   = request.operation == Operation::Preview || request.operation == Operation::PreviewReset;
    if(!resetting) {
        result.config = request.config;
        result.error  = PlaybackStatistics::validateScript(result.config.script);
        if(!result.error.isEmpty()) {
            return update;
        }
    }

    DbTransaction transaction{db()};
    if(!transaction) {
        result.error = tr("Unable to start the statistics operation.");
        return update;
    }

    DbQuery query{db(), u"SELECT %1 FROM TracksView"_s.arg(Database::trackColumns())};
    if(!query.exec()) {
        result.error = tr("Unable to read playback statistics.");
        return update;
    }

    TrackList tracks;
    while(query.next()) {
        tracks.push_back(Database::readTrack(query, metadataStore));
    }

    if(query.lastError().isValid()) {
        result.error = tr("PlaybackStatistics", "Unable to read playback statistics.");
        return update;
    }
    query = DbQuery{};

    std::map<QString, std::set<QString>> destinations;
    std::map<QString, std::map<QString, Track>> groups;
    std::set<QString> selectedKeys;
    std::set<int> changedKeys;
    const std::set<int> selectedIds{request.trackIds.begin(), request.trackIds.end()};

    progress.start(tr("Preparing statistics…"), static_cast<int>(tracks.size()));
    int processed{0};

    for(auto& track : tracks) {
        const QString oldKey = track.statisticsKey();
        if(selectedIds.contains(track.id())) {
            selectedKeys.insert(oldKey);
        }

        bool fallback{false};
        const QString newKey = resetting ? oldKey : PlaybackStatistics::evaluateKey(result.config, track, &fallback);
        result.fallbackTracks += fallback;
        destinations[oldKey].insert(newKey);
        // Each old group contributes once, even if many files share the key
        groups[newKey].try_emplace(oldKey, track);
        if(newKey != oldKey) {
            changedKeys.insert(track.id());
        }
        track.setStatisticsKey(newKey);
        progress.update(++processed);
    }

    for(const auto& targets : destinations | std::views::values) {
        result.splitGroups += targets.size() > 1;
    }

    DbQuery orphanQuery{db(), u"SELECT COUNT(*) FROM TrackStats WHERE StatisticsKey NOT IN "
                              "(SELECT StatisticsKey FROM Tracks)"_s};
    if(!orphanQuery.exec() || !orphanQuery.next()) {
        result.error = tr("Unable to read retained playback statistics.");
        return update;
    }

    result.orphanedGroups = resetting ? 0 : orphanQuery.value(0).toInt();
    orphanQuery           = DbQuery{};

    std::map<QString, Track> replacements;

    progress.start(tr("Preparing statistics groups…"), static_cast<int>(groups.size()));
    processed = 0;

    for(const auto& [key, sources] : groups) {
        if(resetting && !selectedKeys.contains(key)) {
            progress.update(++processed);
            continue;
        }

        if(resetting) {
            const auto& current = sources.begin()->second;
            const bool resetPlaycount
                = request.resetStats.testFlag(Track::Stat::Playcount)
               && (current.playCount() != 0 || current.firstPlayed() != 0 || current.lastPlayed() != 0);
            const bool resetRating = request.resetStats.testFlag(Track::Stat::Rating) && current.rating() > 0;
            const bool resetLoved  = request.resetStats.testFlag(Track::Stat::Loved) && current.isLoved();
            if(!resetPlaycount && !resetRating && !resetLoved) {
                progress.update(++processed);
                continue;
            }
        }

        result.mergedGroups += sources.size() > 1;

        TrackList contributors;
        for(const auto& [oldKey, source] : sources) {
            Track contributor{source};
            if(!resetting && result.config.clearSplitHistory && destinations.at(oldKey).size() > 1) {
                contributor.replaceStatistics(Track{}, Track::Stat::Playcount);
            }
            contributors.push_back(contributor);
        }

        Track replacement
            = resetting ? contributors.front() : PlaybackStatistics::combineStatistics(contributors, result.config);
        if(resetting) {
            replacement.replaceStatistics(Track{}, request.resetStats);
        }
        replacements.emplace(key, std::move(replacement));
        progress.update(++processed);
    }
    result.groups = static_cast<int>(replacements.size());

    std::set<QString> changedGroups;
    int statisticsTracks{0};
    for(const auto& track : tracks) {
        const auto replacement = replacements.find(track.statisticsKey());
        if(replacement == replacements.cend()) {
            continue;
        }

        ++statisticsTracks;
        if(changedKeys.contains(track.id()) || !track.sameStatisticsAs(replacement->second)) {
            ++result.tracks;
            changedGroups.insert(track.statisticsKey());
        }
    }

    const bool configChanged = !resetting && result.config != m_config;
    if(preview || (result.tracks == 0 && result.orphanedGroups == 0 && !configChanged)) {
        return update;
    }

    progress.start(tr("Saving statistics…"), static_cast<int>(changedGroups.size()) + statisticsTracks);
    processed = 0;

    if(configChanged && !writeConfig(db(), result.config)) {
        result.error = tr("Unable to save the statistics configuration.");
        return update;
    }

    for(const auto& key : changedGroups) {
        if(!replaceStats(key, replacements.at(key))) {
            result.error = tr("Unable to write replacement playback statistics.");
            return update;
        }
        progress.update(++processed);
    }

    std::map<int, Track::Stats> fileTagChanges;
    for(auto& track : tracks) {
        const auto replacement = replacements.find(track.statisticsKey());
        if(replacement == replacements.end()) {
            continue;
        }

        Track::Stats fields;
        if(track.playCount() != replacement->second.playCount()) {
            fields |= Track::Stat::Playcount;
        }
        if(track.rating() != replacement->second.rating()) {
            fields |= Track::Stat::Rating;
        }
        if(fields != Track::Stat::None) {
            fileTagChanges.emplace(track.id(), fields);
        }
        track.setAddedTime(replacement->second.addedTime());
        track.replaceStatistics(replacement->second, Track::Stat::All);

        if(changedKeys.contains(track.id())) {
            DbQuery updateKey{db(), u"UPDATE Tracks SET StatisticsKey = :key WHERE TrackID = :id"_s};
            updateKey.bindValue(u":key"_s, track.statisticsKey());
            updateKey.bindValue(u":id"_s, track.id());
            if(!updateKey.exec()) {
                result.error = tr("Unable to update statistics identities.");
                return update;
            }
        }
        progress.update(++processed);
    }

    progress.start(tr("Saving statistics…"));

    if(!resetting && (!changedKeys.empty() || result.orphanedGroups > 0)) {
        DbQuery clear{db(), u"DELETE FROM TrackStats WHERE StatisticsKey NOT IN (SELECT StatisticsKey FROM Tracks)"_s};
        if(!clear.exec()) {
            result.error = tr("Unable to discard retained playback statistics.");
            return update;
        }
    }

    if(result.tracks == 0) {
        if(!transaction.commit()) {
            result.error = tr("Unable to commit playback statistics.");
            return update;
        }
        m_config = result.config;
        return update;
    }

    DbQuery revision{
        db(),
        u"INSERT OR REPLACE INTO Settings (Name, Value) VALUES ('PlaybackStatisticsRevision', "
        "COALESCE((SELECT CAST(Value AS INTEGER) FROM Settings WHERE Name = 'PlaybackStatisticsRevision'), 0) + 1)"_s};
    if(!revision.exec()) {
        result.error = tr("Unable to commit playback statistics.");
        return update;
    }
    const auto committedRevision = readRevision(db());
    if(committedRevision == 0 || !transaction.commit()) {
        result.error = tr("Unable to commit playback statistics.");
        return update;
    }

    m_revision = committedRevision;
    m_config   = result.config;
    for(auto& track : tracks) {
        track.setStatisticsRevision(m_revision);
    }
    update.tracks         = std::move(tracks);
    update.fileTagChanges = std::move(fileTagChanges);
    return update;
}

void PlaybackStatisticsDatabase::cleanupStats() const
{
    static const QString markStatement
        = u"UPDATE TrackStats SET LastSeen = :lastSeen WHERE LastSeen IS NULL AND StatisticsKey NOT IN "
          "(SELECT StatisticsKey FROM Tracks);"_s;

    DbQuery markQuery{db(), markStatement};
    markQuery.bindValue(u":lastSeen"_s, QDateTime::currentMSecsSinceEpoch());
    markQuery.exec();

    static const QString unmarkStatement
        = u"UPDATE TrackStats SET LastSeen = NULL WHERE LastSeen IS NOT NULL AND StatisticsKey IN "
          "(SELECT StatisticsKey FROM Tracks);"_s;

    DbQuery unmarkQuery{db(), unmarkStatement};
    unmarkQuery.exec();

    static const QString statement
        = u"DELETE FROM TrackStats WHERE LastSeen IS NOT NULL AND LastSeen <= :clearInterval AND "
          "StatisticsKey NOT IN (SELECT StatisticsKey FROM Tracks);"_s;

    DbQuery query{db(), statement};

    query.bindValue(u":clearInterval"_s, QDateTime::currentDateTime().addDays(-28).toMSecsSinceEpoch());

    query.exec();
}
} // namespace Fooyin
