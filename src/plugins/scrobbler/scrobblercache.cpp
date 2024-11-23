/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "scrobblercache.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimerEvent>

Q_LOGGING_CATEGORY(SCROBBLER_CACHE, "fy.scrobbler")

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto WriteInterval = 5min;
#else
constexpr auto WriteInterval = 300000;
#endif

namespace Fooyin::Scrobbler {
Metadata::Metadata(const Track& track)
    : title{track.title()}
    , album{track.album()}
    , artist{track.artist()}
    , albumArtist{track.albumArtist()}
    , trackNum{track.trackNumber()}
    , duration{track.duration() / 1000}
{
    if(track.hasExtraTag(u"MUSICBRAINZ_TRACKID"_s)) {
        const auto values = track.extraTag(u"MUSICBRAINZ_TRACKID"_s);
        if(!values.empty()) {
            musicBrainzId = values.front();
        }
    }
}

ScrobblerCache::ScrobblerCache(QString filepath, QObject* parent)
    : QObject{parent}
    , m_filepath{std::move(filepath)}
{
    readCache();
}

ScrobblerCache::~ScrobblerCache()
{
    m_items.clear();
}

void ScrobblerCache::readCache()
{
    QFile file{m_filepath};
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream stream{&file};
    stream.setEncoding(QStringConverter::Encoding::Utf8);

    const QString data = stream.readAll();
    file.close();
    if(data.isEmpty()) {
        return;
    }

    QJsonParseError error;
    const auto json = QJsonDocument::fromJson(data.toUtf8(), &error);

    if(json.isEmpty()) {
        qCWarning(SCROBBLER_CACHE) << "Cache is empty";
        return;
    }

    if(error.error != QJsonParseError::NoError) {
        qCWarning(SCROBBLER_CACHE) << "Cache has no data";
        return;
    }

    if(!json.isObject()) {
        qCWarning(SCROBBLER_CACHE) << "Json doc is not an object";
        return;
    }

    const QJsonObject obj = json.object();
    if(obj.isEmpty()) {
        qCWarning(SCROBBLER_CACHE) << "Json object is empty";
        return;
    }

    if(!obj.contains("Tracks"_L1)) {
        qCWarning(SCROBBLER_CACHE) << "No tracks in json";
        return;
    }

    const QJsonValue tracks = obj.value("Tracks"_L1);
    if(!tracks.isArray()) {
        qCWarning(SCROBBLER_CACHE) << "Tracks is not an array";
        return;
    }

    const QJsonArray trackArray = tracks.toArray();
    if(trackArray.isEmpty()) {
        return;
    }

    for(const auto& value : trackArray) {
        if(!value.isObject()) {
            qCWarning(SCROBBLER_CACHE) << "Array value is not an object";
            continue;
        }

        const QJsonObject trackObj = value.toObject();

        Metadata metadata;
        metadata.title          = trackObj.value("Title"_L1).toString();
        metadata.album          = trackObj.value("Album"_L1).toString();
        metadata.artist         = trackObj.value("Artist"_L1).toString();
        metadata.albumArtist    = trackObj.value("AlbumArtist"_L1).toString();
        metadata.trackNum       = trackObj.value("Track"_L1).toString();
        metadata.duration       = trackObj.value("Duration"_L1).toVariant().toULongLong();
        metadata.musicBrainzId  = trackObj.value("MusicbrainzTrackId"_L1).toString();
        const quint64 timestamp = trackObj.value("Timestamp"_L1).toVariant().toULongLong();

        if(!metadata.isValid()) {
            qCWarning(SCROBBLER_CACHE) << "Metadata in cache data isn't valid";
            continue;
        }

        if(timestamp == 0) {
            qCWarning(SCROBBLER_CACHE) << "Invalid cache data for track" << metadata.title;
            continue;
        }

        m_items.emplace_back(std::make_unique<CacheItem>(metadata, timestamp));
    }
}

CacheItem* ScrobblerCache::add(const Track& track, const uint64_t timestamp)
{
    auto* item = m_items.emplace_back(std::make_unique<CacheItem>(Metadata{track}, timestamp)).get();

    if(!m_writeTimer.isActive()) {
        m_writeTimer.start(WriteInterval, this);
    }

    return item;
}

void ScrobblerCache::remove(CacheItem* item)
{
    std::erase_if(m_items, [item](const auto& cacheItem) { return cacheItem.get() == item; });
}

int ScrobblerCache::count() const
{
    return static_cast<int>(m_items.size());
};

CacheItemList ScrobblerCache::items() const
{
    CacheItemList items;
    std::ranges::transform(m_items, std::back_inserter(items), [](const auto& item) { return item.get(); });
    return items;
}

void ScrobblerCache::flush(const CacheItemList& items)
{
    for(CacheItem* item : items) {
        remove(item);
    }

    if(!m_writeTimer.isActive()) {
        m_writeTimer.start(WriteInterval, this);
    }
}

void ScrobblerCache::writeCache()
{
    if(m_items.empty()) {
        QFile file{m_filepath};
        file.remove();
        return;
    }

    qCDebug(SCROBBLER_CACHE) << "Writing cache to file" << m_filepath;

    QJsonArray array;

    for(const auto& item : std::as_const(m_items)) {
        QJsonObject object;
        object["Title"_L1]              = item->metadata.title;
        object["Album"_L1]              = item->metadata.album;
        object["Artist"_L1]             = item->metadata.artist;
        object["AlbumArtist"_L1]        = item->metadata.albumArtist;
        object["Track"_L1]              = item->metadata.trackNum;
        object["Duration"_L1]           = QJsonValue::fromVariant(static_cast<quint64>(item->metadata.duration));
        object["MusicbrainzTrackId"_L1] = item->metadata.musicBrainzId;
        object["Timestamp"_L1]          = QJsonValue::fromVariant(static_cast<quint64>(item->timestamp));

        array.append(object);
    }

    QJsonObject object;
    object["Tracks"_L1] = array;
    const QJsonDocument doc{object};

    QFile file{m_filepath};
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCDebug(SCROBBLER_CACHE) << "Unable to open cache file" << m_filepath;
        return;
    }

    QTextStream stream{&file};
    stream.setEncoding(QStringConverter::Encoding::Utf8);

    stream << doc.toJson();
}

void ScrobblerCache::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_writeTimer.timerId()) {
        m_writeTimer.stop();
        writeCache();
    }
    QObject::timerEvent(event);
}
} // namespace Fooyin::Scrobbler
