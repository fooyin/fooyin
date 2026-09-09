/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "lovedcache.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTextStream>
#include <QTimerEvent>

#include <algorithm>
#include <ranges>

Q_LOGGING_CATEGORY(LOVED_CACHE, "fy.scrobbler.lovedcache")

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto WriteInterval = 5min;
#else
constexpr auto WriteInterval = 300000;
#endif

namespace Fooyin::Scrobbler {
LovedCache::LovedCache(QString filepath, QObject* parent)
    : QObject{parent}
    , m_filepath{std::move(filepath)}
{
    readCache();
}

void LovedCache::set(Metadata metadata, const bool loved)
{
    const QString key = itemKey(metadata);
    if(key.isEmpty()) {
        return;
    }

    const uint64_t revision = ++m_revision;
    m_items.insert_or_assign(key, LovedItem{key, std::move(metadata), loved, revision});
    scheduleWrite();
}

std::optional<LovedItem> LovedCache::first() const
{
    if(m_items.empty()) {
        return {};
    }
    return m_items.cbegin()->second;
}

void LovedCache::remove(const QString& key, const uint64_t revision)
{
    const auto item = m_items.find(key);
    if(item == m_items.cend() || item->second.revision != revision) {
        return;
    }

    m_items.erase(item);
    scheduleWrite();
}

int LovedCache::count() const
{
    return static_cast<int>(m_items.size());
}

void LovedCache::writeCache()
{
    if(m_items.empty()) {
        QFile::remove(m_filepath);
        return;
    }

    QJsonArray tracks;
    for(const auto& item : m_items | std::views::values) {
        QJsonObject object;
        object["Key"_L1]                = item.key;
        object["Title"_L1]              = item.metadata.title;
        object["Artist"_L1]             = item.metadata.artist;
        object["MusicbrainzTrackId"_L1] = item.metadata.musicBrainzId;
        object["Loved"_L1]              = item.loved;
        object["Revision"_L1]           = QJsonValue::fromVariant(QVariant::fromValue(item.revision));
        tracks.append(object);
    }

    QFile file{m_filepath};
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(LOVED_CACHE) << "Unable to open Loved cache" << m_filepath;
        return;
    }

    QTextStream stream{&file};
    stream.setEncoding(QStringConverter::Encoding::Utf8);
    stream << QJsonDocument{QJsonObject{{"Tracks"_L1, tracks}}}.toJson();
}

void LovedCache::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_writeTimer.timerId()) {
        m_writeTimer.stop();
        writeCache();
    }
    QObject::timerEvent(event);
}

void LovedCache::readCache()
{
    QFile file{m_filepath};
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if(error.error != QJsonParseError::NoError || !document.isObject()) {
        qCWarning(LOVED_CACHE) << "Unable to parse Loved cache" << m_filepath << error.errorString();
        return;
    }

    const QJsonArray tracks = document.object().value("Tracks"_L1).toArray();
    for(const QJsonValue& value : tracks) {
        const QJsonObject object = value.toObject();

        LovedItem item;
        item.key                    = object.value("Key"_L1).toString();
        item.metadata.title         = object.value("Title"_L1).toString();
        item.metadata.artist        = object.value("Artist"_L1).toString();
        item.metadata.musicBrainzId = object.value("MusicbrainzTrackId"_L1).toString();
        item.loved                  = object.value("Loved"_L1).toBool();
        item.revision               = object.value("Revision"_L1).toVariant().toULongLong();

        if(item.key.isEmpty() || item.metadata.title.isEmpty() || item.metadata.artist.isEmpty()) {
            continue;
        }

        m_revision = std::max(m_revision, item.revision);
        m_items.insert_or_assign(item.key, std::move(item));
    }
}

void LovedCache::scheduleWrite()
{
    if(!m_writeTimer.isActive()) {
        m_writeTimer.start(WriteInterval, this);
    }
}

QString LovedCache::itemKey(const Metadata& metadata)
{
    const QString musicBrainzId = metadata.musicBrainzId.trimmed();
    if(!musicBrainzId.isEmpty()) {
        return u"mbid:"_s + musicBrainzId.toLower();
    }

    if(metadata.artist.isEmpty() || metadata.title.isEmpty()) {
        return {};
    }
    return u"metadata:"_s + metadata.artist.toCaseFolded() + u'\x1f' + metadata.title.toCaseFolded();
}
} // namespace Fooyin::Scrobbler
