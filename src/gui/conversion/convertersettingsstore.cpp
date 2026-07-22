/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "convertersettingsstore.h"

#include <core/coresettings.h>
#include <utils/settings/settingsmanager.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

constexpr auto EncoderProfilesKey   = "Converter/EncoderProfiles";
constexpr auto ConversionPresetsKey = "Converter/Presets";
constexpr auto LastUsedPresetKey    = "Converter/LastUsedPreset";
constexpr auto DataVersion          = 1;

namespace Fooyin {
namespace {
QJsonObject encoderProfileToJson(const EncoderProfile& profile)
{
    return {
        {u"id"_s, profile.id},
        {u"name"_s, profile.name},
        {u"extension"_s, profile.extension},
        {u"container"_s, profile.containerName},
        {u"codec"_s, profile.codecName},
        {u"mode"_s, static_cast<int>(profile.mode)},
        {u"bitrateKbps"_s, profile.bitrateKbps},
        {u"quality"_s, profile.quality},
        {u"compressionLevel"_s, profile.compressionLevel},
    };
}

EncoderProfile encoderProfileFromJson(const QJsonObject& object)
{
    EncoderProfile profile;
    profile.id               = object.value(u"id"_s).toString();
    profile.name             = object.value(u"name"_s).toString();
    profile.extension        = object.value(u"extension"_s).toString();
    profile.containerName    = object.value(u"container"_s).toString();
    profile.codecName        = object.value(u"codec"_s).toString();
    profile.mode             = static_cast<EncoderMode>(object.value(u"mode"_s).toInt());
    profile.bitrateKbps      = object.value(u"bitrateKbps"_s).toInt();
    profile.quality          = object.value(u"quality"_s).toDouble();
    profile.compressionLevel = object.value(u"compressionLevel"_s).toInt(-1);
    return profile;
}

QJsonObject dspDefinitionToJson(const Engine::DspDefinition& dsp)
{
    return {
        {u"id"_s, dsp.id},
        {u"name"_s, dsp.name},
        {u"hasSettings"_s, dsp.hasSettings},
        {u"enabled"_s, dsp.enabled},
        {u"instanceId"_s, QString::number(dsp.instanceId)},
        {u"settings"_s, QString::fromUtf8(dsp.settings.toBase64())},
    };
}

QJsonArray dspChainToJson(const Engine::DspChain& chain)
{
    QJsonArray result;
    for(const auto& dsp : chain) {
        result.push_back(dspDefinitionToJson(dsp));
    }
    return result;
}

Engine::DspChain dspChainFromJson(const QJsonArray& array)
{
    Engine::DspChain result;

    for(const auto& value : array) {
        const QJsonObject object = value.toObject();

        Engine::DspDefinition dsp;
        dsp.id          = object.value(u"id"_s).toString();
        dsp.name        = object.value(u"name"_s).toString();
        dsp.hasSettings = object.value(u"hasSettings"_s).toBool();
        dsp.enabled     = object.value(u"enabled"_s).toBool(true);
        dsp.instanceId  = object.value(u"instanceId"_s).toString().toULongLong();
        dsp.settings    = QByteArray::fromBase64(object.value(u"settings"_s).toString().toUtf8());

        if(!dsp.id.isEmpty()) {
            result.push_back(std::move(dsp));
        }
    }

    return result;
}

QJsonObject conversionPresetToJson(const StoredConversionPreset& stored)
{
    const auto& preset = stored.preset;
    return {
        {u"name"_s, stored.name},
        {u"id"_s, preset.id},
        {u"profile"_s, encoderProfileToJson(preset.encoder.profile)},
        {u"sampleFormat"_s, static_cast<int>(preset.encoder.outputSampleFormat)},
        {u"dither"_s, static_cast<int>(preset.encoder.ditherMode)},
        {u"destinationMode"_s, static_cast<int>(preset.destination.mode)},
        {u"fixedFolder"_s, preset.destination.fixedFolder},
        {u"filenamePattern"_s, preset.destination.filenamePattern},
        {u"existingFileMode"_s, static_cast<int>(preset.destination.existingFileMode)},
        {u"outputStyle"_s, static_cast<int>(preset.destination.outputStyle)},
        {u"transferMetadata"_s, preset.processing.transferMetadata},
        {u"transferRating"_s, preset.processing.transferRating},
        {u"transferPlaycount"_s, preset.processing.transferPlaycount},
        {u"transferPictures"_s, preset.processing.transferPictures},
        {u"replayGainMode"_s, static_cast<int>(preset.processing.replayGainMode)},
        {u"replayGainPreventClipping"_s, preset.processing.replayGainPreventClipping},
        {u"replayGainPreampDb"_s, preset.processing.replayGainPreampDb},
        {u"replayGainWithoutInfoPreampDb"_s, preset.processing.replayGainWithoutInfoPreampDb},
        {u"dspChain"_s, dspChainToJson(preset.processing.dspChain)},
        {u"preserveDspStateBetweenTracks"_s, preset.processing.preserveDspStateBetweenTracks},
        {u"previewEnabled"_s, preset.preview.enabled},
        {u"previewLengthPercentage"_s, preset.preview.lengthPercentage},
        {u"copyFilesPattern"_s, preset.other.copyFilesPattern},
        {u"verifyOutput"_s, preset.other.verifyOutput},
        {u"showReport"_s, stored.showReport},
    };
}

StoredConversionPreset conversionPresetFromJson(const QJsonObject& object)
{
    StoredConversionPreset stored;

    stored.name                              = object.value(u"name"_s).toString();
    stored.preset.id                         = object.value(u"id"_s).toString();
    stored.preset.name                       = stored.name;
    stored.preset.encoder.profile            = encoderProfileFromJson(object.value(u"profile"_s).toObject());
    stored.preset.encoder.outputSampleFormat = static_cast<SampleFormat>(object.value(u"sampleFormat"_s).toInt());
    stored.preset.encoder.ditherMode         = static_cast<DitherMode>(object.value(u"dither"_s).toInt());
    stored.preset.destination.mode           = static_cast<DestinationMode>(object.value(u"destinationMode"_s).toInt());
    stored.preset.destination.fixedFolder    = object.value(u"fixedFolder"_s).toString();
    stored.preset.destination.filenamePattern = object.value(u"filenamePattern"_s).toString(u"%title%"_s);
    stored.preset.destination.existingFileMode
        = static_cast<ExistingFileMode>(object.value(u"existingFileMode"_s).toInt());
    stored.preset.destination.outputStyle      = static_cast<OutputStyle>(object.value(u"outputStyle"_s).toInt());
    stored.preset.processing.transferMetadata  = object.value(u"transferMetadata"_s).toBool(true);
    stored.preset.processing.transferRating    = object.value(u"transferRating"_s).toBool(true);
    stored.preset.processing.transferPlaycount = object.value(u"transferPlaycount"_s).toBool(false);
    stored.preset.processing.transferPictures  = object.value(u"transferPictures"_s).toBool(true);
    stored.preset.processing.replayGainMode    = static_cast<ConversionReplayGainMode>(
        object.value(u"replayGainMode"_s).toInt(static_cast<int>(ConversionReplayGainMode::None)));
    stored.preset.processing.replayGainPreventClipping = object.value(u"replayGainPreventClipping"_s).toBool(true);
    stored.preset.processing.replayGainPreampDb        = object.value(u"replayGainPreampDb"_s).toDouble();
    stored.preset.processing.replayGainWithoutInfoPreampDb
        = object.value(u"replayGainWithoutInfoPreampDb"_s).toDouble();
    stored.preset.processing.dspChain = dspChainFromJson(object.value(u"dspChain"_s).toArray());
    stored.preset.processing.preserveDspStateBetweenTracks
        = object.value(u"preserveDspStateBetweenTracks"_s).toBool(false);
    stored.preset.preview.enabled          = object.value(u"previewEnabled"_s).toBool(false);
    stored.preset.preview.lengthPercentage = object.value(u"previewLengthPercentage"_s).toInt(20);
    stored.preset.other.copyFilesPattern   = object.value(u"copyFilesPattern"_s).toString();
    stored.preset.other.verifyOutput       = object.value(u"verifyOutput"_s).toBool(false);
    stored.showReport                      = object.value(u"showReport"_s).toBool(true);

    return stored;
}

QByteArray serialise(const QJsonArray& entries)
{
    return QJsonDocument{QJsonObject{{u"version"_s, DataVersion}, {u"entries"_s, entries}}}.toJson(
        QJsonDocument::Compact);
}

QJsonArray entriesFromData(const QByteArray& data)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if(error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();
    if(root.value(u"version"_s).toInt() != DataVersion) {
        return {};
    }

    return root.value(u"entries"_s).toArray();
}
} // namespace

namespace ConverterSettings {
EncoderProfile applyStoredEncoderProfile(EncoderProfile profile, const StoredEncoderProfile& stored)
{
    profile.name             = stored.profile.name;
    profile.mode             = stored.profile.mode;
    profile.bitrateKbps      = stored.profile.bitrateKbps;
    profile.quality          = stored.profile.quality;
    profile.compressionLevel = stored.profile.compressionLevel;
    return profile;
}

std::vector<StoredEncoderProfile> encoderProfiles()
{
    const FySettings settings;
    return deserialiseEncoderProfiles(settings.value(EncoderProfilesKey).toByteArray());
}

void setEncoderProfiles(const std::vector<StoredEncoderProfile>& profiles)
{
    FySettings settings;
    settings.setValue(EncoderProfilesKey, serialiseEncoderProfiles(profiles));
}

std::vector<StoredConversionPreset> conversionPresets()
{
    const FySettings settings;
    return deserialiseConversionPresets(settings.value(ConversionPresetsKey).toByteArray());
}

void setConversionPresets(const std::vector<StoredConversionPreset>& presets)
{
    FySettings settings;
    settings.setValue(ConversionPresetsKey, serialiseConversionPresets(presets));
}

std::optional<StoredConversionPreset> lastUsedConversionPreset()
{
    const FySettings settings;
    auto presets = deserialiseConversionPresets(settings.value(LastUsedPresetKey).toByteArray());
    if(presets.empty()) {
        return {};
    }
    return std::move(presets.front());
}

void setLastUsedConversionPreset(const StoredConversionPreset& preset)
{
    FySettings settings;
    settings.setValue(LastUsedPresetKey, serialiseConversionPresets({preset}));
}

QByteArray serialiseEncoderProfiles(const std::vector<StoredEncoderProfile>& profiles)
{
    QJsonArray entries;
    for(const auto& stored : profiles) {
        entries.push_back(QJsonObject{
            {u"storageId"_s, stored.storageId},
            {u"baseEncoderId"_s, stored.baseEncoderId},
            {u"overridesBuiltIn"_s, stored.overridesBuiltIn},
            {u"profile"_s, encoderProfileToJson(stored.profile)},
        });
    }
    return serialise(entries);
}

std::vector<StoredEncoderProfile> deserialiseEncoderProfiles(const QByteArray& data)
{
    std::vector<StoredEncoderProfile> profiles;

    const auto entries = entriesFromData(data);
    for(const auto& entry : entries) {
        const QJsonObject object = entry.toObject();

        StoredEncoderProfile stored;
        stored.storageId        = object.value(u"storageId"_s).toString();
        stored.baseEncoderId    = object.value(u"baseEncoderId"_s).toString();
        stored.overridesBuiltIn = object.value(u"overridesBuiltIn"_s).toBool();
        stored.profile          = encoderProfileFromJson(object.value(u"profile"_s).toObject());

        if(!stored.storageId.isEmpty() && !stored.baseEncoderId.isEmpty() && !stored.profile.name.isEmpty()) {
            profiles.push_back(std::move(stored));
        }
    }
    return profiles;
}

QByteArray serialiseConversionPresets(const std::vector<StoredConversionPreset>& presets)
{
    QJsonArray entries;
    for(const auto& stored : presets) {
        entries.push_back(conversionPresetToJson(stored));
    }
    return serialise(entries);
}

std::vector<StoredConversionPreset> deserialiseConversionPresets(const QByteArray& data)
{
    std::vector<StoredConversionPreset> presets;

    const auto entries = entriesFromData(data);
    for(const auto& entry : entries) {
        StoredConversionPreset stored = conversionPresetFromJson(entry.toObject());

        if(!stored.name.isEmpty() && !stored.preset.encoder.profile.id.isEmpty()) {
            presets.push_back(std::move(stored));
        }
    }
    return presets;
}
} // namespace ConverterSettings
} // namespace Fooyin
