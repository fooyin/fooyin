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

#include "opusreplaygainutils.h"

#include <core/constants.h>

#include <cmath>
#include <limits>

using namespace Qt::StringLiterals;

constexpr auto OpusReplayGainOffsetDb = 5.0;

namespace Fooyin::RGScanner {
namespace {
std::optional<int16_t> adjustCommentGain(std::optional<int16_t> gain, int deltaQ78)
{
    if(!gain.has_value()) {
        return {};
    }

    const auto newGain = static_cast<long long>(*gain) - deltaQ78;
    if(newGain < std::numeric_limits<int16_t>::min() || newGain > std::numeric_limits<int16_t>::max()) {
        return {};
    }

    return static_cast<int16_t>(newGain);
}

int headerDeltaFromReplayGain(int16_t commentGainQ78, double targetVolumeDb)
{
    return commentGainQ78 + q78FromDb(OpusReplayGainOffsetDb + (targetVolumeDb - ReplayGainReferenceDb));
}
} // namespace

bool isWritableOpusTrack(const Track& track)
{
    return !track.isInArchive() && track.isOpus();
}

void removeReplayGainTags(Track& track)
{
    track.setRGTrackGain(Constants::InvalidGain);
    track.setRGAlbumGain(Constants::InvalidGain);

    static const QStringList replayGainTags = {
        QStringLiteral("REPLAYGAIN_TRACK_GAIN"),
        QStringLiteral("REPLAYGAIN_ALBUM_GAIN"),
        QStringLiteral("----:COM.APPLE.ITUNES:REPLAYGAIN_TRACK_GAIN"),
        QStringLiteral("----:COM.APPLE.ITUNES:REPLAYGAIN_ALBUM_GAIN"),
        QStringLiteral("R128_TRACK_GAIN"),
        QStringLiteral("R128_ALBUM_GAIN"),
    };

    for(const QString& tag : replayGainTags) {
        track.removeExtraTag(tag);
    }
}

void removeReplayGainInfoFromFile(Track& track)
{
    removeReplayGainTags(track);
    track.setRGTrackPeak(Constants::InvalidPeak);
    track.setRGAlbumPeak(Constants::InvalidPeak);
    track.removeExtraTag(QStringLiteral("REPLAYGAIN_TRACK_PEAK"));
    track.removeExtraTag(QStringLiteral("REPLAYGAIN_ALBUM_PEAK"));
    track.removeExtraTag(QStringLiteral("----:COM.APPLE.ITUNES:REPLAYGAIN_TRACK_PEAK"));
    track.removeExtraTag(QStringLiteral("----:COM.APPLE.ITUNES:REPLAYGAIN_ALBUM_PEAK"));

    if(isWritableOpusTrack(track)) {
        track.setOpusHeaderGainQ78(0);
    }
}

float q78ToDb(int16_t gainQ78)
{
    return static_cast<float>(gainQ78) / 256.0F;
}

int q78FromDb(double gainDb)
{
    return static_cast<int>(std::lround(gainDb * 256.0));
}

std::optional<int16_t> dbToQ78(double gainDb)
{
    const auto gainQ78 = std::lround(gainDb * 256.0);
    if(gainQ78 < std::numeric_limits<int16_t>::min() || gainQ78 > std::numeric_limits<int16_t>::max()) {
        return {};
    }

    return static_cast<int16_t>(gainQ78);
}

std::optional<int16_t> replayGainToOpusR128Q78(float gainDb)
{
    return dbToQ78(static_cast<double>(gainDb) - OpusReplayGainOffsetDb);
}

std::optional<OpusGainState> applyHeaderGainMode(const OpusGainState& current, OpusHeaderGainMode mode,
                                                 std::optional<int16_t> explicitHeaderGain,
                                                 const OpusHeaderGainOptions& options)
{
    OpusGainState updated{current};
    int headerDeltaQ78{0};
    int commentDeltaQ78{0};

    switch(mode) {
        case OpusHeaderGainMode::Track:
            if(!current.trackGain.has_value()) {
                return current;
            }

            headerDeltaQ78  = headerDeltaFromReplayGain(*current.trackGain, options.targetVolumeDb);
            commentDeltaQ78 = headerDeltaQ78;

            if(options.louderOnly && headerDeltaQ78 <= 0) {
                return current;
            }
            break;
        case OpusHeaderGainMode::Album:
            if(!current.albumGain.has_value()) {
                return current;
            }

            headerDeltaQ78  = headerDeltaFromReplayGain(*current.albumGain, options.targetVolumeDb);
            commentDeltaQ78 = headerDeltaQ78;

            if(options.louderOnly && headerDeltaQ78 <= 0) {
                return current;
            }
            break;
        case OpusHeaderGainMode::Explicit:
            if(!explicitHeaderGain.has_value()) {
                return {};
            }
            headerDeltaQ78  = *explicitHeaderGain - current.headerGain;
            commentDeltaQ78 = headerDeltaQ78;
            break;
        case OpusHeaderGainMode::Clear:
            headerDeltaQ78  = -current.headerGain;
            commentDeltaQ78 = headerDeltaQ78;
            break;
    }

    const auto newHeader = static_cast<long long>(current.headerGain) + headerDeltaQ78;
    if(newHeader < std::numeric_limits<int16_t>::min() || newHeader > std::numeric_limits<int16_t>::max()) {
        return {};
    }
    updated.headerGain = static_cast<int16_t>(newHeader);

    if(const auto newTrackGain = adjustCommentGain(current.trackGain, commentDeltaQ78); current.trackGain.has_value()) {
        if(!newTrackGain.has_value()) {
            return {};
        }
        updated.trackGain = newTrackGain;
    }

    if(const auto newAlbumGain = adjustCommentGain(current.albumGain, commentDeltaQ78); current.albumGain.has_value()) {
        if(!newAlbumGain.has_value()) {
            return {};
        }
        updated.albumGain = newAlbumGain;
    }

    return updated;
}
} // namespace Fooyin::RGScanner
