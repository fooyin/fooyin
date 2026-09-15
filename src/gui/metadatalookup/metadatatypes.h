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

#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>

#include <unordered_map>
#include <vector>

namespace Fooyin {
enum class LookupMode : uint8_t
{
    ArtistAlbum,
    DiscToc,
    ReleaseId,
    ReleaseGroupId,
};

struct LookupQuery
{
    LookupMode mode{LookupMode::ArtistAlbum};
    QString artist;
    QString album;
    QString discToc;
    QString discId;
    QString identifier;
};

struct ArtistCredit
{
    QString name;
    QString joinPhrase;
    QString id;

    bool operator==(const ArtistCredit&) const = default;
};

struct ReleaseSummary
{
    QString sourceId;
    QString id;
    QString title;
    std::vector<ArtistCredit> artistCredit;
    QString date;
    QString originalDate;
    QString country;
    QString status;
    QString disambiguation;
    QString barcode;
    QStringList labels;
    QStringList catalogNumbers;
    QStringList formats;
    QStringList releaseTypes;
    std::unordered_map<QString, QStringList> identifiers;
    int discCount{0};
};

struct ReleaseTrack
{
    QString title;
    std::vector<ArtistCredit> artistCredit;
    QStringList isrcs;
    QStringList genres;
    std::unordered_map<QString, QStringList> identifiers;
    int mediumPosition{0};
    int position{0};
    QString number;
    int total{0};
    int64_t durationMs{-1};
};

struct ReleaseMedium
{
    int position{0};
    QString title;
    QString format;
    QStringList discIds;
    std::vector<ReleaseTrack> tracks;
};

struct Release
{
    ReleaseSummary summary;
    std::vector<ReleaseMedium> media;
    QStringList genres;
};

[[nodiscard]] inline QString artistCreditText(const std::vector<ArtistCredit>& credits)
{
    QString result;

    for(const auto& credit : credits) {
        result += credit.name;
        result += credit.joinPhrase;
    }

    return result;
}

[[nodiscard]] inline QStringList artistCreditNames(const std::vector<ArtistCredit>& credits)
{
    QStringList result;

    for(const auto& credit : credits) {
        if(!credit.name.isEmpty()) {
            result.push_back(credit.name);
        }
    }

    return result;
}

[[nodiscard]] inline QStringList artistCreditIds(const std::vector<ArtistCredit>& credits)
{
    QStringList result;

    for(const auto& credit : credits) {
        if(!credit.id.isEmpty()) {
            result.push_back(credit.id);
        }
    }

    return result;
}

[[nodiscard]] inline std::vector<const ReleaseTrack*> flattenedTracks(const Release& release)
{
    std::vector<const ReleaseTrack*> result;

    for(const auto& medium : release.media) {
        for(const auto& track : medium.tracks) {
            result.push_back(&track);
        }
    }

    return result;
}
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::ReleaseSummary)
Q_DECLARE_METATYPE(Fooyin::Release)
