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

#include <core/engine/conversion/conversiondefs.h>

#include <QByteArray>
#include <QString>

#include <array>
#include <optional>
#include <ranges>
#include <vector>

namespace Fooyin {
class SettingsManager;

struct StoredEncoderProfile
{
    QString storageId;
    QString baseEncoderId;
    EncoderProfile profile;
    bool overridesBuiltIn{false};
};

struct StoredConversionPreset
{
    QString name;
    ConversionPreset preset;
    bool showReport{true};
    bool showOutputFiles{false};
};

namespace ConverterSettings {
constexpr std::array DefaultEncoderProfileIds{"flac", "ffmpeg-flac", "ffmpeg-wav"};

template <typename Range, typename Projection>
auto findDefaultEncoderProfile(Range& profiles, Projection projection)
{
    const auto end = std::ranges::end(profiles);
    for(const char* id : DefaultEncoderProfileIds) {
        const auto profile = std::ranges::find(profiles, QLatin1StringView{id}, projection);
        if(profile != end) {
            return profile;
        }
    }
    return end;
}

EncoderProfile applyStoredEncoderProfile(EncoderProfile profile, const StoredEncoderProfile& stored);

std::vector<StoredEncoderProfile> encoderProfiles();
void setEncoderProfiles(const std::vector<StoredEncoderProfile>& profiles);

std::vector<StoredConversionPreset> conversionPresets();
void setConversionPresets(const std::vector<StoredConversionPreset>& presets);

std::optional<StoredConversionPreset> lastUsedConversionPreset();
void setLastUsedConversionPreset(const StoredConversionPreset& preset);

QByteArray serialiseEncoderProfiles(const std::vector<StoredEncoderProfile>& profiles);
std::vector<StoredEncoderProfile> deserialiseEncoderProfiles(const QByteArray& data);
QByteArray serialiseConversionPresets(const std::vector<StoredConversionPreset>& presets);
std::vector<StoredConversionPreset> deserialiseConversionPresets(const QByteArray& data);
} // namespace ConverterSettings
} // namespace Fooyin
