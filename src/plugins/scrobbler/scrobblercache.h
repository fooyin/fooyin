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

#pragma once

#include <core/track.h>

#include <QBasicTimer>
#include <QObject>

namespace Fooyin::Scrobbler {
class Metadata
{
public:
    Metadata() = default;
    explicit Metadata(const Track& track);

    QString title;
    QString album;
    QString artist;
    QString albumArtist;
    QString trackNum;
    uint64_t duration{0};
    QString musicBrainzId;
    QString musicBrainzAlbumId;

    [[nodiscard]] bool isValid() const
    {
        return !title.isEmpty() && !artist.isEmpty() && duration >= 30;
    }
};

struct CacheItem
{
    Metadata metadata;
    uint64_t timestamp{0};
    bool submitted{false};
    bool hasError{false};
};
using CacheItemUPtrList = std::vector<std::unique_ptr<CacheItem>>;
using CacheItemList     = std::vector<CacheItem*>;

class ScrobblerCache : public QObject
{
    Q_OBJECT

public:
    explicit ScrobblerCache(QString filepath, QObject* parent);
    ~ScrobblerCache() override;

    void readCache();

    CacheItem* add(const Track& track, uint64_t timestamp);
    void remove(CacheItem* item);
    [[nodiscard]] int count() const;
    [[nodiscard]] CacheItemList items() const;
    void flush(const CacheItemList& items);

    void writeCache();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    QString m_filepath;
    QBasicTimer m_writeTimer;
    CacheItemUPtrList m_items;
};
} // namespace Fooyin::Scrobbler
