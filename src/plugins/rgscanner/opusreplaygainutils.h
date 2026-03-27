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

#include <core/track.h>

namespace Fooyin {
class SettingsManager;

namespace RGScanner {
inline constexpr auto ReplayGainReferenceDb = 89.0;

enum class OpusHeaderGainMode : uint8_t
{
    Track = 0,
    Album,
    Explicit,
    Clear,
};

struct OpusGainState
{
    int16_t headerGain{0};
    std::optional<int16_t> trackGain;
    std::optional<int16_t> albumGain;

    [[nodiscard]] bool operator==(const OpusGainState& other) const = default;
};

struct OpusHeaderGainOptions
{
    double targetVolumeDb{ReplayGainReferenceDb};
    bool louderOnly{false};
};

[[nodiscard]] bool isWritableOpusTrack(const Track& track);
void removeReplayGainTags(Track& track);
void removeReplayGainInfoFromFile(Track& track);

[[nodiscard]] float q78ToDb(int16_t gainQ78);
[[nodiscard]] int q78FromDb(double gainDb);
[[nodiscard]] std::optional<int16_t> dbToQ78(double gainDb);
[[nodiscard]] std::optional<int16_t> replayGainToOpusR128Q78(float gainDb);

[[nodiscard]] std::optional<OpusGainState> applyHeaderGainMode(const OpusGainState& current, OpusHeaderGainMode mode,
                                                               std::optional<int16_t> explicitHeaderGain,
                                                               const OpusHeaderGainOptions& options);
} // namespace RGScanner
} // namespace Fooyin
