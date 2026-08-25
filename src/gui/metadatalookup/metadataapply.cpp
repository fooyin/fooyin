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

#include "metadataapply.h"

#include <QMap>

#include <functional>
#include <ranges>
#include <set>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QMap<QString, QString> displayMetadata(const Track& track)
{
    QMap<QString, QString> result = track.metadata();
    for(const auto& [key, values] : track.extraTags()) {
        result.insert(key, values.join(u"; "_s));
    }
    return result;
}

template <typename Value, typename Getter, typename Setter>
void setBuiltIn(Track& track, const Value& value, ExistingMetadataPolicy policy, Getter getter, Setter setter)
{
    if(value.isEmpty() || (policy == ExistingMetadataPolicy::FillMissing && !std::invoke(getter, track).isEmpty())) {
        return;
    }
    setter(track, value);
}

void setExtra(Track& track, const QString& tag, const QStringList& values, ExistingMetadataPolicy policy)
{
    if(values.empty() || (policy == ExistingMetadataPolicy::FillMissing && track.hasExtraTag(tag))) {
        return;
    }
    track.replaceExtraTag(tag, values);
}

void setExtra(Track& track, const QString& tag, const QString& value, ExistingMetadataPolicy policy)
{
    setExtra(track, tag, value.isEmpty() ? QStringList{} : QStringList{value}, policy);
}

void setIdentifiers(Track& track, const std::unordered_map<QString, QStringList>& identifiers,
                    ExistingMetadataPolicy policy)
{
    for(const auto& [tag, value] : identifiers) {
        setExtra(track, tag, value, policy);
    }
}

QString mediumFormat(const Release& release, int position)
{
    const auto it = std::ranges::find(release.media, position, &ReleaseMedium::position);
    return it != release.media.cend() ? it->format : QString{};
}

QString mediumTitle(const Release& release, int position)
{
    const auto it = std::ranges::find(release.media, position, &ReleaseMedium::position);
    return it != release.media.cend() ? it->title : QString{};
}
} // namespace

MetadataApplyResult applyReleaseMetadata(const TrackList& localTracks, const Release& release,
                                         const std::vector<TrackMatch>& matches, const MetadataApplyOptions& options)
{
    MetadataApplyResult result;

    const auto remoteTracks = flattenedTracks(release);
    const auto albumArtists = artistCreditNames(release.summary.artistCredit);
    const QString date      = options.useOriginalReleaseDate && !release.summary.originalDate.isEmpty()
                                ? release.summary.originalDate
                                : release.summary.date;

    for(const TrackMatch& match : matches) {
        if(match.localIndex >= localTracks.size() || !match.remoteIndex || *match.remoteIndex >= remoteTracks.size()) {
            continue;
        }

        const Track& original       = localTracks.at(match.localIndex);
        const ReleaseTrack& remote  = *remoteTracks.at(*match.remoteIndex);
        const auto originalMetadata = displayMetadata(original);
        Track updated{original};

        if(options.policy == ExistingMetadataPolicy::WipeWritableTags) {
            updated.clearWritableTags();
        }

        setBuiltIn(updated, remote.title, options.policy, &Track::title,
                   [](Track& track, const QString& value) { track.setTitle(value); });
        setBuiltIn(updated, artistCreditNames(remote.artistCredit), options.policy, &Track::artists,
                   [](Track& track, const QStringList& value) { track.setArtists(value); });
        setBuiltIn(updated, release.summary.title, options.policy, &Track::album,
                   [](Track& track, const QString& value) { track.setAlbum(value); });
        setBuiltIn(updated, albumArtists, options.policy, &Track::albumArtists,
                   [](Track& track, const QStringList& value) { track.setAlbumArtists(value); });
        setBuiltIn(updated, remote.number.isEmpty() ? QString::number(remote.position) : remote.number, options.policy,
                   &Track::trackNumber, [](Track& track, const QString& value) { track.setTrackNumber(value); });
        setBuiltIn(updated, QString::number(remote.total), options.policy, &Track::trackTotal,
                   [](Track& track, const QString& value) { track.setTrackTotal(value); });
        setBuiltIn(updated, QString::number(remote.mediumPosition), options.policy, &Track::discNumber,
                   [](Track& track, const QString& value) { track.setDiscNumber(value); });

        const int discTotal
            = release.summary.discCount > 0 ? release.summary.discCount : static_cast<int>(release.media.size());
        setBuiltIn(updated, QString::number(discTotal), options.policy, &Track::discTotal,
                   [](Track& track, const QString& value) { track.setDiscTotal(value); });

        setBuiltIn(updated, date, options.policy, &Track::date,
                   [](Track& track, const QString& value) { track.setDate(value); });

        if(options.writeGenres) {
            const QStringList genres = !remote.genres.empty() ? remote.genres : release.genres;
            setBuiltIn(updated, genres, options.policy, &Track::genres,
                       [](Track& track, const QStringList& value) { track.setGenres(value); });
        }

        if(options.writeReleaseIds) {
            setIdentifiers(updated, release.summary.identifiers, options.policy);
            setIdentifiers(updated, remote.identifiers, options.policy);
        }

        setExtra(updated, u"RELEASECOUNTRY"_s, release.summary.country, options.policy);
        setExtra(updated, u"RELEASESTATUS"_s, release.summary.status, options.policy);
        setExtra(updated, u"RELEASETYPE"_s, release.summary.releaseTypes, options.policy);
        setExtra(updated, u"ORIGINALDATE"_s, release.summary.originalDate, options.policy);
        setExtra(updated, u"LABEL"_s, release.summary.labels, options.policy);
        setExtra(updated, u"CATALOGNUMBER"_s, release.summary.catalogNumbers, options.policy);
        setExtra(updated, u"BARCODE"_s, release.summary.barcode, options.policy);
        setExtra(updated, u"ISRC"_s, remote.isrcs, options.policy);
        setExtra(updated, u"MEDIA"_s, mediumFormat(release, remote.mediumPosition), options.policy);
        setExtra(updated, u"DISCSUBTITLE"_s, mediumTitle(release, remote.mediumPosition), options.policy);

        const auto updatedMetadata = displayMetadata(updated);
        std::set<QString> fields;
        for(auto it = originalMetadata.cbegin(); it != originalMetadata.cend(); ++it) {
            fields.insert(it.key());
        }
        for(auto it = updatedMetadata.cbegin(); it != updatedMetadata.cend(); ++it) {
            fields.insert(it.key());
        }

        bool changed{false};
        for(const QString& field : fields) {
            const QString before = originalMetadata.value(field);
            const QString after  = updatedMetadata.value(field);
            if(before != after) {
                result.changes.push_back(
                    {.localIndex = match.localIndex, .field = field, .before = before, .after = after});
                changed = true;
            }
        }
        if(changed) {
            result.trackIndices.push_back(match.localIndex);
            result.tracks.push_back(std::move(updated));
        }
    }

    return result;
}
} // namespace Fooyin
