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

#include "taglyrics.h"

#include "settings/lyricssettings.h"

#include <utils/settings/settingsmanager.h>

namespace Fooyin::Lyrics {
QString TagLyrics::name() const
{
    return QStringLiteral("Metadata Tags");
}

bool TagLyrics::isLocal() const
{
    return true;
}

void TagLyrics::search(const SearchParams& params)
{
    std::vector<LyricData> data;

    const auto searchTags = settings()->value<Settings::Lyrics::SearchTags>();
    for(const QString& tag : searchTags) {
        const QStringList lyrics = params.track.extraTag(tag);
        if(!lyrics.empty()) {
            LyricData lyricData;
            lyricData.title  = params.title;
            lyricData.album  = params.album;
            lyricData.artist = params.artist;
            lyricData.data   = lyrics.constFirst().toUtf8();
            data.push_back(lyricData);
        }
    }

    emit searchResult(data);
}
} // namespace Fooyin::Lyrics
