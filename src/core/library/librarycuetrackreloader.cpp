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

#include "librarycuetrackreloader.h"

#include <core/engine/audioloader.h>
#include <core/playlist/parsers/cueparser.h>
#include <core/playlist/playlistparser.h>
#include <core/trackmetadatastore.h>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <map>
#include <optional>
#include <ranges>
#include <tuple>
#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
struct CueReloadGroup
{
    QString source;
    int subsong{0};
    bool embedded{false};
    TrackList tracks;
};

using CueReloadGroups = std::map<std::tuple<bool, QString, int>, CueReloadGroup>;

CueReloadGroups groupCueTracks(const TrackList& tracks)
{
    CueReloadGroups groups;

    for(const Track& track : tracks) {
        const bool embedded  = track.hasEmbeddedCue();
        const QString source = embedded ? track.filepath() : track.cuePath();
        const int subsong    = embedded ? track.subsong() : 0;
        auto& group          = groups[{embedded, source, subsong}];
        group.source         = source;
        group.subsong        = subsong;
        group.embedded       = embedded;
        group.tracks.push_back(track);
    }

    return groups;
}

uint64_t fileModifiedTime(const QString& path)
{
    const QDateTime modifiedTime = QFileInfo{path}.lastModified();
    return modifiedTime.isValid() ? static_cast<uint64_t>(modifiedTime.toMSecsSinceEpoch()) : 0;
}

bool groupWasModified(const CueReloadGroup& group)
{
    const uint64_t cueModified = group.embedded ? 0 : fileModifiedTime(group.source);
    return std::ranges::any_of(group.tracks, [cueModified](const Track& track) {
        return track.modifiedTime() < std::max(cueModified, fileModifiedTime(physicalTrackPath(track)));
    });
}

std::vector<std::optional<qsizetype>> matchReloadedTracks(const TrackList& reloadedTracks,
                                                          const TrackList& existingTracks)
{
    std::vector<std::optional<qsizetype>> matches(reloadedTracks.size());
    std::vector claimed(existingTracks.size(), false);

    for(qsizetype i{0}; i < std::ssize(reloadedTracks); ++i) {
        for(qsizetype j{0}; j < std::ssize(existingTracks); ++j) {
            if(!claimed[j] && existingTracks[j].sameIdentityAs(reloadedTracks[i])) {
                matches[i] = j;
                claimed[j] = true;
                break;
            }
        }
    }

    for(qsizetype i{0}; i < std::ssize(reloadedTracks); ++i) {
        if(matches[i].has_value() || reloadedTracks[i].trackNumber().isEmpty()) {
            continue;
        }

        for(qsizetype j{0}; j < std::ssize(existingTracks); ++j) {
            if(!claimed[j] && normalisePath(existingTracks[j].filepath()) == normalisePath(reloadedTracks[i].filepath())
               && existingTracks[j].trackNumber() == reloadedTracks[i].trackNumber()) {
                matches[i] = j;
                claimed[j] = true;
                break;
            }
        }
    }

    return matches;
}

TrackList readCueTracks(const CueReloadGroup& group, AudioLoader* audioLoader,
                        const std::shared_ptr<TrackMetadataStore>& metadataStore,
                        const CueTrackReloader::ContinueHandler& shouldContinue)
{
    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [audioLoader, metadataStore, &readEntry, &shouldContinue](const Track& playlistTrack) {
        if(!shouldContinue()) {
            readEntry.cancel = true;
            return playlistTrack;
        }

        Track readTrack{playlistTrack};
        readTrack.setMetadataStore(metadataStore);
        if(!audioLoader->readTrackMetadata(readTrack)) {
            return playlistTrack;
        }

        readFileProperties(readTrack);
        return readTrack;
    };
    readEntry.canLoadTrack = [audioLoader](const Track& track) {
        return static_cast<bool>(audioLoader->loadDecoderForTrack(track).decoder);
    };

    CueParser parser;

    TrackList cueTracks;
    if(group.embedded) {
        Track parentTrack{group.source, group.subsong, metadataStore};
        parentTrack = readEntry.readTrack(parentTrack);
        if(readEntry.cancel) {
            return {};
        }

        const QStringList cueSheets = parentTrack.extraTag(u"CUESHEET"_s);
        if(cueSheets.empty()) {
            return {};
        }

        QByteArray cueData = cueSheets.front().toUtf8();
        QBuffer buffer{&cueData};
        if(buffer.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const auto readParent = readEntry.readTrack;
            readEntry.readTrack   = [parentTrack, readParent](const Track& track) {
                if(normalisePath(track.filepath()) == normalisePath(parentTrack.filepath())
                   && track.subsong() == parentTrack.subsong()) {
                    return parentTrack;
                }
                return readParent(track);
            };
            cueTracks = parser.readPlaylist(&buffer, group.source, {}, readEntry, false);
        }
    }
    else {
        QFile cueFile{group.source};
        if(!cueFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }

        const QDir cueDir = QFileInfo{group.source}.dir();
        cueTracks         = parser.readPlaylist(&cueFile, group.source, cueDir, readEntry, false);
    }

    for(auto& cueTrack : cueTracks) {
        cueTrack.setMetadataStore(metadataStore);
    }

    return cueTracks;
}
} // namespace

CueTrackReloader::CueTrackReloader(AudioLoader* audioLoader, std::shared_ptr<TrackMetadataStore> metadataStore)
    : m_audioLoader{audioLoader}
    , m_metadataStore{std::move(metadataStore)}
{ }

CueTrackReloadResult CueTrackReloader::reload(const TrackList& tracks, const bool onlyModified,
                                              const TrackReloadOptions& options, const ContinueHandler& shouldContinue,
                                              const ProgressHandler& reportProgress) const
{
    CueTrackReloadResult result;

    for(const CueReloadGroup& group : groupCueTracks(tracks) | std::views::values) {
        if(!shouldContinue()) {
            result.cancelled = true;
            break;
        }

        reportProgress(static_cast<int>(group.tracks.size()), group.source);

        if(onlyModified && !groupWasModified(group)) {
            continue;
        }

        TrackList reloadedTracks = readCueTracks(group, m_audioLoader, m_metadataStore, shouldContinue);
        if(!shouldContinue()) {
            result.cancelled = true;
            break;
        }

        const auto matches = matchReloadedTracks(reloadedTracks, group.tracks);
        for(qsizetype i{0}; i < std::ssize(reloadedTracks); ++i) {
            if(!matches[i].has_value()) {
                continue;
            }

            Track& reloadedTrack  = reloadedTracks[i];
            const Track& existing = group.tracks[*matches[i]];

            reloadedTrack.setLibraryId(existing.libraryId());
            reloadedTrack.setAddedTime(existing.addedTime());
            reloadedTrack.setIsEnabled(existing.isEnabled());
            mergeReloadedTrackStats(reloadedTrack, existing, options);
            reloadedTrack.generateHash();

            if(existing.isInDatabase()) {
                reloadedTrack.setId(existing.id());
                result.updatedTracks.push_back(reloadedTrack);
            }
            else {
                result.addedTracks.push_back(reloadedTrack);
            }
        }
    }

    return result;
}
} // namespace Fooyin
