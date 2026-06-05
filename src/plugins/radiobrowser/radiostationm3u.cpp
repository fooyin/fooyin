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

#include "radiostationm3u.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QTextStream>

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
namespace {
QString extInfName(const QString& line)
{
    const qsizetype comma = line.indexOf(u',');
    if(comma < 0) {
        return {};
    }
    return line.sliced(comma + 1).trimmed();
}

QString stationUuid(const QString& line)
{
    static const QRegularExpression uuidRegex{u"STATIONUUID\\s*=\\s*\"([^\"]+)\""_s,
                                              QRegularExpression::CaseInsensitiveOption};

    const QRegularExpressionMatch match = uuidRegex.match(line);
    return match.hasMatch() ? match.captured(1).trimmed() : QString{};
}

QString escapedM3uValue(QString value)
{
    value.replace(u'\\', u"\\\\"_s);
    value.replace(u'"', u"\\\""_s);
    return value;
}

void appendEntry(std::vector<RadioStationM3uEntry>& entries, const QString& uuid, QString name, QString streamUrl)
{
    streamUrl = streamUrl.trimmed();
    if(streamUrl.isEmpty()) {
        return;
    }

    if(name.trimmed().isEmpty()) {
        name = streamUrl;
    }

    entries.emplace_back(RadioStationM3uEntry{.uuid = uuid.trimmed(), .name = name.trimmed(), .streamUrl = streamUrl});
}
} // namespace

RadioStationM3uReadResult readRadioStationM3u(const QString& filePath)
{
    QFile file{filePath};
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        RadioStationM3uReadResult result;
        result.error = QObject::tr("Unable to open file.");
        return result;
    }

    RadioStationM3uReadResult result;
    QString pendingUuid;
    QString pendingName;

    QTextStream stream{&file};
    stream.setEncoding(QStringConverter::Utf8);

    while(!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if(line.isEmpty()) {
            continue;
        }

        if(line.startsWith("#EXT-X-MEDIA:"_L1)) {
            pendingUuid = stationUuid(line);
            continue;
        }

        if(line.startsWith("#EXTINF:"_L1)) {
            pendingName = extInfName(line);
            continue;
        }

        if(line.startsWith(u'#')) {
            continue;
        }

        appendEntry(result.entries, std::exchange(pendingUuid, {}), std::exchange(pendingName, {}), line);
    }

    if(result.entries.empty()) {
        result.error = QObject::tr("No stations found in file.");
    }

    return result;
}

bool writeRadioStationM3u(const QString& filePath, const RadioStationList& stations, QString* error)
{
    const QFileInfo fileInfo{filePath};
    const QDir dir = fileInfo.dir();
    if(!dir.exists() && !QDir{}.mkpath(dir.absolutePath())) {
        if(error) {
            *error = QObject::tr("Unable to create destination folder.");
        }
        return false;
    }

    QFile file{filePath};
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if(error) {
            *error = QObject::tr("Unable to open file for writing.");
        }
        return false;
    }

    QTextStream stream{&file};
    stream.setEncoding(QStringConverter::Utf8);
    stream << "#EXTM3U\n";
    stream << "#EXTENC:UTF-8\n";
    stream << "#PLAYLIST:My Stations\n\n";

    for(const RadioStation& station : stations) {
        const QString streamUrl = station.streamUrl.trimmed();
        if(streamUrl.isEmpty()) {
            continue;
        }

        if(!station.uuid.isEmpty()) {
            stream << "#EXT-X-MEDIA:STATIONUUID=\"" << escapedM3uValue(station.uuid) << "\"\n";
        }
        stream << "#EXTINF:-1," << station.name << "\n";
        stream << streamUrl << "\n\n";
    }

    return true;
}
} // namespace Fooyin::RadioBrowser
