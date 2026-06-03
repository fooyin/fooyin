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

#include "locallyrics.h"

#include "settings/lyricssettings.h"

#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace Fooyin::Lyrics {
QString LocalLyrics::name() const
{
    return u"Local Files"_s;
}

bool LocalLyrics::isLocal() const
{
    return true;
}

void LocalLyrics::search(const SearchParams& params)
{
    QStringList filters;

    const auto paths = settings()->fileValue(Settings::Paths, Defaults::paths()).toStringList();
    for(const QString& path : paths) {
        filters.emplace_back(m_parser.evaluate(path.trimmed(), params.track));
    }

    QStringList lrcPaths;

    for(const auto& filter : filters) {
        const QStringList fileList
            = Utils::File::filesFromWildcardPath(filter, QDir::Files | QDir::Hidden, QDir::NoSort);
        for(const QString& file : fileList) {
            lrcPaths.emplace_back(file);
        }
    }

    std::vector<LyricData> data;

    for(const QString& file : lrcPaths) {
        QFile lrcFile{file};
        if(!lrcFile.open(QIODevice::ReadOnly)) {
            qCInfo(LYRICS) << "Could not open file" << file << "for reading:" << lrcFile.errorString();
        }
        else {
            LyricData lyricData;
            lyricData.data = toUtf8(&lrcFile);

            if(!lyricData.data.isEmpty()) {
                lyricData.path   = file;
                lyricData.title  = params.title;
                lyricData.album  = params.album;
                lyricData.artist = params.artist;
                data.push_back(lyricData);
            }
        }
    }

    Q_EMIT searchResult({data});
}
} // namespace Fooyin::Lyrics
