/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <core/scripting/scriptparser.h>

#include <QBasicTimer>
#include <QObject>

namespace Fooyin {
class Track;
namespace Scrobbler {
class Metadata
{
public:
    Metadata() = default;
    explicit Metadata(ScriptParser* parser, SettingsManager* settings, const Track& track);

    QString title;
    QString album;
    QString artist;
    QString albumArtist;
    QString trackNum;
    uint64_t duration{0};
    bool radio{false};
    QString musicBrainzId;
    QString musicBrainzAlbumId;

    bool operator==(const Metadata&) const = default;

    [[nodiscard]] bool isValid() const
    {
        return !title.trimmed().isEmpty() && !artist.trimmed().isEmpty()
            && (duration >= 30 || (radio && duration == 0));
    }
};

struct CacheItem
{
    Metadata metadata;
    uint64_t timestamp{0};
    QString account;
    bool submitted{false};
    bool hasError{false};
    QString id;
};
using CacheItemUPtrList = std::vector<std::unique_ptr<CacheItem>>;
using CacheItemList     = std::vector<CacheItem*>;

class ScrobblerCache : public QObject
{
    Q_OBJECT

public:
    explicit ScrobblerCache(QString filepath, SettingsManager* settings, QObject* parent);
    ~ScrobblerCache() override;

    void readCache();

    [[nodiscard]] QString account() const;
    void setAccount(const QString& account);

    CacheItem* add(const Track& track, uint64_t timestamp);
    void remove(CacheItem* item);
    bool updateMetadata(const QString& id, const Track& track);
    [[nodiscard]] int count() const;
    [[nodiscard]] CacheItemList items() const;
    void flush(const CacheItemList& items);

    void writeCache();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    SettingsManager* m_settings;
    ScriptParser m_scriptParser;

    QString m_filepath;
    QString m_account;
    QBasicTimer m_writeTimer;
    CacheItemUPtrList m_items;
};
} // namespace Scrobbler
} // namespace Fooyin