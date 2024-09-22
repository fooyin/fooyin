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
    if(track.hasExtraTag(QStringLiteral("MUSICBRAINZ_TRACKID"))) {
        const auto values = track.extraTag(QStringLiteral("MUSICBRAINZ_TRACKID"));
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

    if(!obj.contains(u"Tracks")) {
        qCWarning(SCROBBLER_CACHE) << "No tracks in json";
        return;
    }

    const QJsonValue tracks = obj.value(u"Tracks");
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
        metadata.title          = trackObj.value(u"Title").toString();
        metadata.album          = trackObj.value(u"Album").toString();
        metadata.artist         = trackObj.value(u"Artist").toString();
        metadata.albumArtist    = trackObj.value(u"AlbumArtist").toString();
        metadata.trackNum       = trackObj.value(u"Track").toString();
        metadata.duration       = trackObj.value(u"Duration").toVariant().toULongLong();
        metadata.musicBrainzId  = trackObj.value(u"MusicbrainzTrackId").toString();
        const quint64 timestamp = trackObj.value(u"Timestamp").toVariant().toULongLong();

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
        object[u"Title"]              = item->metadata.title;
        object[u"Album"]              = item->metadata.album;
        object[u"Artist"]             = item->metadata.artist;
        object[u"AlbumArtist"]        = item->metadata.albumArtist;
        object[u"Track"]              = item->metadata.trackNum;
        object[u"Duration"]           = QJsonValue::fromVariant(static_cast<quint64>(item->metadata.duration));
        object[u"MusicbrainzTrackId"] = item->metadata.musicBrainzId;
        object[u"Timestamp"]          = QJsonValue::fromVariant(static_cast<quint64>(item->timestamp));

        array.append(object);
    }

    QJsonObject object;
    object[u"Tracks"] = array;
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
