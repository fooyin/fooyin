/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <core/engine/conversion/conversiondefs.h>

#include <QByteArray>
#include <QString>

#include <optional>
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
};

namespace ConverterSettings {
constexpr auto DefaultEncoderProfileId = "ffmpeg-wav";

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
