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

#include <core/playhistory/playhistorystats.h>

#include <QDateTime>
#include <QSet>
#include <QTime>

#include <algorithm>
#include <map>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
using Category = PlayHistoryStats::Category;

struct TrackInfo
{
    // Metadata of the most recent listen
    const PlayHistoryEntry* entry{nullptr};
    int count{0};
    qint64 firstInRange{0};
    // Most listens in a single day, and that day
    int bestDayCount{0};
    QDate bestDay;
    int currentDayCount{0};
    QDate currentDay;
    // Longest run of consecutive days with a listen
    int streak{0};
    int bestStreak{0};
    QDate streakEnd;
    QDate bestStreakEnd;
};

PlayHistoryStatsTrack toStatsTrack(const TrackInfo& info)
{
    PlayHistoryStatsTrack track;
    track.hash   = info.entry->hash;
    track.title  = info.entry->title;
    track.artist = info.entry->artists.join(u", "_s);
    track.album  = info.entry->album;
    track.count  = info.count;
    return track;
}

template <typename Compare>
void keepTop(std::vector<PlayHistoryStatsTrack>& tracks, int count, Compare compare)
{
    const auto limit = static_cast<size_t>(std::max(count, 0));
    if(tracks.size() > limit) {
        std::partial_sort(tracks.begin(), tracks.begin() + static_cast<std::ptrdiff_t>(limit), tracks.end(), compare);
        tracks.resize(limit);
    }
    else {
        std::ranges::sort(tracks, compare);
    }
}

QStringList categoryValues(const PlayHistoryEntry& entry, Category category)
{
    switch(category) {
        case Category::Genre:
            return entry.genres;
        case Category::Artist:
            return entry.artists;
        case Category::AlbumArtist:
            return entry.albumArtists;
        case Category::Composer:
            return entry.composers;
        case Category::Album:
            return entry.album.isEmpty() ? QStringList{} : QStringList{entry.album};
    }
    return {};
}

PlayHistoryStatsCategory rankCategory(const QHash<QString, int>& counts, int topCount)
{
    std::vector<PlayHistoryStatsRank> ranks;
    ranks.reserve(static_cast<size_t>(counts.size()));
    for(auto it = counts.cbegin(); it != counts.cend(); ++it) {
        ranks.push_back({.key = it.key(), .count = it.value()});
    }

    // Tie-break on the name so the ranking is stable between runs
    std::ranges::sort(ranks, [](const auto& lhs, const auto& rhs) {
        return lhs.count != rhs.count ? lhs.count > rhs.count : lhs.key < rhs.key;
    });

    PlayHistoryStatsCategory category;
    category.unique = static_cast<int>(ranks.size());

    const auto limit = static_cast<size_t>(topCount);
    if(ranks.size() > limit) {
        for(size_t i{limit}; i < ranks.size(); ++i) {
            category.otherListens += ranks[i].count;
        }
        ranks.resize(limit);
    }
    category.top = std::move(ranks);

    return category;
}

int bitrateBucket(int kbps)
{
    if(kbps < 128) {
        return 0;
    }
    if(kbps < 256) {
        return 1;
    }
    if(kbps < 320) {
        return 2;
    }
    return 3;
}
} // namespace

int PlayHistoryStats::listensOnDay(const QDate& day) const
{
    const auto index = firstDay.daysTo(day);
    if(index < 0 || index >= static_cast<qint64>(listensPerDay.size())) {
        return 0;
    }
    return listensPerDay[static_cast<size_t>(index)];
}

PlayHistoryStats computePlayHistoryStats(const PlayHistoryData& data, QDate firstDay, QDate lastDay,
                                         const PlayHistoryStatsOptions& options)
{
    PlayHistoryStats stats;

    if(data.oldestTimestamp > 0) {
        const QDate oldest = QDateTime::fromMSecsSinceEpoch(data.oldestTimestamp).date();
        if(oldest > firstDay) {
            firstDay = oldest;
        }
    }
    if(lastDay < firstDay) {
        lastDay = firstDay;
    }

    stats.firstDay = firstDay;
    stats.lastDay  = lastDay;
    stats.listensPerDay.assign(static_cast<size_t>(stats.totalDays()), 0);
    stats.peakDay = firstDay;

    const qint64 rangeStart = firstDay.startOfDay().toMSecsSinceEpoch();
    const qint64 rangeEnd   = lastDay.addDays(1).startOfDay().toMSecsSinceEpoch();

    QHash<QString, TrackInfo> tracks;
    std::array<QHash<QString, int>, PlayHistoryStats::CategoryCount> categoryCounts;
    QHash<QString, int> artistCounts;
    std::map<int, int> decadeCounts;
    QHash<QString, std::pair<QSet<QString>, int>> albumHeard;
    std::array<QHash<QString, std::pair<const PlayHistoryEntry*, int>>, 12> monthCounts;

    qint64 sessionStart{0};
    qint64 sessionEnd{0};
    int sessionListens{0};
    const auto finishSession = [&stats, &sessionStart, &sessionEnd, &sessionListens]() {
        if(sessionListens < 2) {
            return;
        }
        if(!stats.longestSession || sessionEnd - sessionStart > stats.longestSession->durationMs()) {
            stats.longestSession = PlayHistoryStatsSession{sessionStart, sessionEnd, sessionListens};
        }
    };

    QDate currentDay;
    qint64 dayFirstMs{0};
    qint64 dayLastMs{0};
    qint64 firstMinuteSum{0};
    qint64 lastMinuteSum{0};
    const auto finishDay = [&]() {
        if(!currentDay.isValid()) {
            return;
        }
        firstMinuteSum += QDateTime::fromMSecsSinceEpoch(dayFirstMs).time().msecsSinceStartOfDay() / 60000;
        lastMinuteSum += QDateTime::fromMSecsSinceEpoch(dayLastMs).time().msecsSinceStartOfDay() / 60000;
    };

    const PlayHistoryEntry* previous{nullptr};

    for(const PlayHistoryEntry& entry : data.entries) {
        if(entry.timestamp < rangeStart || entry.timestamp >= rangeEnd) {
            continue;
        }

        const QDateTime local = QDateTime::fromMSecsSinceEpoch(entry.timestamp);
        const QDate day       = local.date();
        const int hour        = local.time().hour();
        // Qt numbers Monday as 1 and Sunday as 7
        const int weekday = day.dayOfWeek() % 7;

        if(day != currentDay) {
            finishDay();
            currentDay = day;
            dayFirstMs = entry.timestamp;
        }
        dayLastMs = entry.timestamp;

        ++stats.totalListens;
        const auto dayIndex = static_cast<size_t>(firstDay.daysTo(day));
        if(++stats.listensPerDay[dayIndex] > stats.peakDayListens) {
            stats.peakDayListens = stats.listensPerDay[dayIndex];
            stats.peakDay        = day;
        }

        ++stats.hours[static_cast<size_t>(hour)];
        ++stats.weekdays[static_cast<size_t>(weekday)];
        ++stats.weekHours[static_cast<size_t>(weekday * 24 + hour)];
        ++stats.months[static_cast<size_t>(day.month() - 1)];

        if(previous && previous->hash == entry.hash) {
            ++stats.repeatListens;
        }
        previous = &entry;

        if(sessionListens == 0 || entry.timestamp - sessionEnd > PlayHistoryStats::SessionGapMs) {
            finishSession();
            sessionStart   = entry.timestamp;
            sessionListens = 0;
        }
        sessionEnd = entry.timestamp;
        ++sessionListens;

        auto trackIt = tracks.find(entry.hash);
        if(trackIt == tracks.end()) {
            trackIt                      = tracks.insert(entry.hash, TrackInfo{});
            trackIt.value().firstInRange = entry.timestamp;
        }
        TrackInfo& track = trackIt.value();
        track.entry      = &entry;
        ++track.count;

        if(track.currentDay != day) {
            track.currentDay      = day;
            track.currentDayCount = 0;
            // Consecutive days extend the streak, any gap restarts it
            track.streak    = track.streakEnd.isValid() && track.streakEnd.addDays(1) == day ? track.streak + 1 : 1;
            track.streakEnd = day;
            if(track.streak > track.bestStreak) {
                track.bestStreak    = track.streak;
                track.bestStreakEnd = day;
            }
        }
        if(++track.currentDayCount > track.bestDayCount) {
            track.bestDayCount = track.currentDayCount;
            track.bestDay      = day;
        }

        for(int i{0}; i < PlayHistoryStats::CategoryCount; ++i) {
            const auto values = categoryValues(entry, static_cast<Category>(i));
            for(const QString& value : values) {
                if(!value.isEmpty()) {
                    ++categoryCounts[static_cast<size_t>(i)][value];
                }
            }
        }

        if(options.monthlyTop) {
            auto& month = monthCounts[static_cast<size_t>(day.month() - 1)][entry.hash];
            month.first = &entry;
            ++month.second;
        }

        if(entry.year > 0) {
            ++decadeCounts[(entry.year / 10) * 10];
        }
        if(entry.lossless == 1) {
            ++stats.losslessListens;
        }
        else if(entry.lossless == 0) {
            ++stats.lossyListens;
        }
        if(entry.bitrate > 0) {
            ++stats.bitrateBuckets[static_cast<size_t>(bitrateBucket(entry.bitrate))];
        }

        const QString albumKey = entry.albumKey();
        if(!albumKey.isEmpty()) {
            auto& album = albumHeard[albumKey];
            album.first.insert(entry.hash);
            ++album.second;
        }

        stats.listenSeconds += static_cast<qint64>(entry.duration / 1000);
    }
    finishSession();
    finishDay();

    stats.uniqueTracks = static_cast<int>(tracks.size());

    for(int i{0}; i < PlayHistoryStats::CategoryCount; ++i) {
        stats.categories[static_cast<size_t>(i)]
            = rankCategory(categoryCounts[static_cast<size_t>(i)], PlayHistoryStats::MaxTop);
    }
    stats.uniqueArtists = stats.category(Category::Artist).unique;
    stats.uniqueGenres  = stats.category(Category::Genre).unique;
    stats.uniqueAlbums  = stats.category(Category::Album).unique;

    for(const int listens : stats.listensPerDay) {
        if(listens > 0) {
            ++stats.daysWithListens;
        }
    }

    // Streaks of consecutive days with at least one listen
    {
        int run{0};
        for(size_t i{0}; i < stats.listensPerDay.size(); ++i) {
            if(stats.listensPerDay[i] > 0) {
                ++run;
                if(!stats.longestStreak || run > stats.longestStreak->days) {
                    stats.longestStreak = PlayHistoryStatsStreak{run, firstDay.addDays(static_cast<qint64>(i))};
                }
            }
            else {
                run = 0;
            }
        }

        // The streak is still current if nothing has been played yet today
        auto i = static_cast<qsizetype>(stats.listensPerDay.size()) - 1;
        if(i >= 0 && stats.listensPerDay[static_cast<size_t>(i)] == 0) {
            --i;
        }
        while(i >= 0 && stats.listensPerDay[static_cast<size_t>(i)] > 0) {
            ++stats.currentStreak;
            --i;
        }
    }

    if(stats.daysWithListens > 0) {
        stats.avgFirstMinute = static_cast<int>(firstMinuteSum / stats.daysWithListens);
        stats.avgLastMinute  = static_cast<int>(lastMinuteSum / stats.daysWithListens);
    }

    std::vector<PlayHistoryStatsTrack> ranked;
    std::vector<PlayHistoryStatsTrack> newTracks;
    std::vector<PlayHistoryStatsTrack> obsessions;
    std::vector<PlayHistoryStatsTrack> rediscoveries;
    ranked.reserve(static_cast<size_t>(tracks.size()));

    const PlayHistoryEntry* oldestFavouriteEntry{nullptr};
    qint64 oldestFavouriteMs{0};
    int oldestFavouriteCount{0};
    QSet<QString> artistsInRange;

    for(auto it = tracks.cbegin(); it != tracks.cend(); ++it) {
        const TrackInfo& info = it.value();
        const QString& hash   = it.key();

        const PlayHistoryStatsTrack track = toStatsTrack(info);
        ranked.push_back(track);

        if(info.count == 1) {
            ++stats.oneTimeTracks;
        }
        if(info.count >= PlayHistoryStats::HeavyRotationListens) {
            ++stats.heavyRotationTracks;
        }

        if(info.bestDayCount >= 2) {
            auto obsession      = track;
            obsession.count     = info.bestDayCount;
            obsession.timestamp = info.bestDay.startOfDay().toMSecsSinceEpoch();
            obsessions.push_back(std::move(obsession));
        }

        if(info.bestStreak >= 2
           && (!stats.longestTrackStreak || info.bestStreak > stats.longestTrackStreak->days
               || (info.bestStreak == stats.longestTrackStreak->days
                   && info.count > stats.longestTrackStreak->count))) {
            auto streak              = track;
            streak.days              = info.bestStreak;
            streak.timestamp         = info.bestStreakEnd.startOfDay().toMSecsSinceEpoch();
            stats.longestTrackStreak = std::move(streak);
        }

        const qint64 firstEver = data.firstListenByHash.value(hash, 0);
        if(firstEver >= rangeStart) {
            ++stats.newTracks;
            auto discovery      = track;
            discovery.timestamp = firstEver;
            newTracks.push_back(std::move(discovery));
        }
        else if(firstEver > 0 && info.count >= 2
                && (!oldestFavouriteEntry || firstEver < oldestFavouriteMs
                    || (firstEver == oldestFavouriteMs && info.count > oldestFavouriteCount))) {
            oldestFavouriteEntry = info.entry;
            oldestFavouriteMs    = firstEver;
            oldestFavouriteCount = info.count;
        }

        if(const auto before = data.lastListenBeforeByHash.constFind(hash);
           before != data.lastListenBeforeByHash.cend()) {
            const auto gapDays = (info.firstInRange - before.value()) / (24LL * 60 * 60 * 1000);
            if(gapDays >= PlayHistoryStats::RediscoveryGapDays) {
                auto rediscovery = track;
                rediscovery.days = static_cast<int>(gapDays);
                rediscoveries.push_back(std::move(rediscovery));
            }
        }

        for(const QString& artist : info.entry->artists) {
            artistsInRange.insert(artist);
        }
    }

    const auto byCount = [](const PlayHistoryStatsTrack& lhs, const PlayHistoryStatsTrack& rhs) {
        if(lhs.count != rhs.count) {
            return lhs.count > rhs.count;
        }
        // Keep results stable between runs
        return lhs.hash < rhs.hash;
    };

    keepTop(ranked, options.topCount, byCount);
    stats.topTracks = std::move(ranked);
    keepTop(newTracks, options.topCount, byCount);
    stats.newTracksTop = std::move(newTracks);
    keepTop(obsessions, PlayHistoryStats::MaxObsessions, byCount);
    stats.obsessions = std::move(obsessions);
    keepTop(rediscoveries, PlayHistoryStats::MaxRediscoveries, byCount);
    stats.rediscoveries = std::move(rediscoveries);

    if(oldestFavouriteEntry) {
        PlayHistoryStatsTrack favourite;
        favourite.hash        = oldestFavouriteEntry->hash;
        favourite.title       = oldestFavouriteEntry->title;
        favourite.artist      = oldestFavouriteEntry->artists.join(u", "_s);
        favourite.album       = oldestFavouriteEntry->album;
        favourite.count       = oldestFavouriteCount;
        favourite.timestamp   = oldestFavouriteMs;
        stats.oldestFavourite = std::move(favourite);
    }

    // Artists whose very first listen is within the range
    for(const QString& artist : std::as_const(artistsInRange)) {
        if(data.firstListenByArtist.value(artist, 0) >= rangeStart) {
            ++stats.newArtists;
            stats.newArtistsTop.push_back(
                {.key = artist, .count = categoryCounts[static_cast<size_t>(Category::Artist)].value(artist)});
        }
    }
    std::ranges::sort(stats.newArtistsTop, [](const auto& lhs, const auto& rhs) {
        return lhs.count != rhs.count ? lhs.count > rhs.count : lhs.key < rhs.key;
    });
    if(stats.newArtistsTop.size() > PlayHistoryStats::MaxNewArtists) {
        stats.newArtistsTop.resize(PlayHistoryStats::MaxNewArtists);
    }

    for(const auto& [decade, listens] : decadeCounts) {
        stats.decades.push_back({.key = QString::number(decade), .count = listens});
    }

    for(auto it = albumHeard.cbegin(); it != albumHeard.cend(); ++it) {
        const int total = options.albumTrackCounts.value(it.key(), 0);
        if(total >= 2 && it.value().first.size() >= total) {
            const QString name = it.key().section(QLatin1Char{'\037'}, 0, 0);
            stats.completedAlbums.push_back({.name = name, .tracks = total, .listens = it.value().second});
        }
    }
    std::ranges::sort(stats.completedAlbums, [](const auto& lhs, const auto& rhs) {
        return lhs.listens != rhs.listens ? lhs.listens > rhs.listens : lhs.name < rhs.name;
    });

    if(options.monthlyTop) {
        for(size_t month{0}; month < monthCounts.size(); ++month) {
            const PlayHistoryEntry* bestEntry{nullptr};
            int bestCount{0};
            for(auto it = monthCounts[month].cbegin(); it != monthCounts[month].cend(); ++it) {
                if(it.value().second > bestCount
                   || (it.value().second == bestCount && bestEntry && it.key() < bestEntry->hash)) {
                    bestEntry = it.value().first;
                    bestCount = it.value().second;
                }
            }
            if(bestEntry) {
                PlayHistoryStatsTrack best;
                best.hash               = bestEntry->hash;
                best.title              = bestEntry->title;
                best.artist             = bestEntry->artists.join(u", "_s);
                best.album              = bestEntry->album;
                best.count              = bestCount;
                stats.monthlyTop[month] = std::move(best);
            }
        }
    }

    return stats;
}
} // namespace Fooyin
