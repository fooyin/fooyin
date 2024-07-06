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

#include "wavebardatabase.h"

#include <core/track.h>
#include <utils/crypto.h>
#include <utils/database/dbquery.h>
#include <utils/datastream.h>

namespace {
using Fooyin::WaveBar::WaveformData;

QDataStream& operator>>(QDataStream& stream, std::vector<WaveformData<int16_t>::ChannelData>& data)
{
    quint32 size;
    stream >> size;

    data.reserve(size);

    while(size > 0) {
        --size;

        WaveformData<int16_t>::ChannelData channel;
        Fooyin::operator>>(stream, channel.max);
        Fooyin::operator>>(stream, channel.min);
        Fooyin::operator>>(stream, channel.rms);
        data.emplace_back(channel);
    }
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const std::vector<WaveformData<int16_t>::ChannelData>& data)
{
    stream << static_cast<quint32>(data.size());

    for(const auto& channel : data) {
        Fooyin::operator<<(stream, channel.max);
        Fooyin::operator<<(stream, channel.min);
        Fooyin::operator<<(stream, channel.rms);
    }

    return stream;
}

QByteArray serialiseData(const WaveformData<int16_t>& data)
{
    QByteArray out;
    QDataStream stream{&out, QDataStream::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << data.channelData;

    out = qCompress(out, 9);

    return out;
}

void deserialiseData(const QByteArray& cacheData, WaveformData<int16_t>& data)
{
    QByteArray in = qUncompress(cacheData);
    QDataStream stream{&in, QDataStream::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream >> data.channelData;
}
} // namespace

namespace Fooyin::WaveBar {
void WaveBarDatabase::initialiseDatabase() const
{
    const auto statement = QStringLiteral("CREATE TABLE IF NOT EXISTS WaveCache ("
                                          "TrackKey TEXT PRIMARY KEY, "
                                          "Data BLOB);");

    DbQuery query{db(), statement};
    query.exec();
}

bool WaveBarDatabase::existsInCache(const QString& key) const
{
    const auto statement = QStringLiteral("SELECT COUNT(*) FROM WaveCache WHERE TrackKey = :trackKey;");

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":trackKey"), key);

    if(query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

bool WaveBarDatabase::loadCachedData(const QString& key, WaveformData<int16_t>& data) const
{
    const auto statement = QStringLiteral("SELECT Data FROM WaveCache WHERE TrackKey = :trackKey;");

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":trackKey"), key);

    if(query.exec() && query.next()) {
        const QByteArray cacheData = query.value(0).toByteArray();
        deserialiseData(cacheData, data);
        return true;
    }

    return false;
}

bool WaveBarDatabase::storeInCache(const QString& key, const WaveformData<int16_t>& data) const
{
    const auto statement
        = QStringLiteral("INSERT OR REPLACE INTO WaveCache (TrackKey, Data) VALUES (:trackKey, :data);");

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":trackKey"), key);
    query.bindValue(QStringLiteral(":data"), serialiseData(data));

    return query.exec();
}

bool WaveBarDatabase::removeFromCache(const QString& key) const
{
    const auto statement = QStringLiteral("DELETE FROM WaveCache WHERE TrackKey = :trackKey;");

    DbQuery query{db(), statement};
    query.bindValue(QStringLiteral(":trackKey"), key);

    return query.exec();
}

bool WaveBarDatabase::removeFromCache(const QStringList& keys) const
{
    const QString statement = QStringLiteral("DELETE FROM WaveCache WHERE TrackKey IN (:keys);");

    DbQuery query{db(), statement};
    query.bindValue(QStringLiteral(":keys"), keys);

    return query.exec();
}

bool WaveBarDatabase::clearCache() const
{
    const auto statement = QStringLiteral("DELETE FROM WaveCache;");

    DbQuery query{db(), statement};
    DbQuery cleanQuery{db(), QStringLiteral("VACUUM")};

    return query.exec() && cleanQuery.exec();
}

QString WaveBarDatabase::cacheKey(const Track& track)
{
    return cacheKey(track, track.channels());
}

QString WaveBarDatabase::cacheKey(const Track& track, int channels)
{
    return Utils::generateHash(track.hash(), QString::number(track.duration()), QString::number(track.sampleRate()),
                               QString::number(channels));
}
} // namespace Fooyin::WaveBar
