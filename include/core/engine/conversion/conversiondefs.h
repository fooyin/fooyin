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

#include "fycore_export.h"

#include <core/engine/audioformat.h>
#include <core/engine/enginedefs.h>
#include <core/track.h>

#include <QString>

namespace Fooyin {
enum class DitherMode : uint8_t
{
    Never = 0,
    LossySourceOnly,
    Always,
};

enum class EncoderMode : uint8_t
{
    Default = 0,
    VariableBitrate,
    ConstrainedVariableBitrate,
    ConstantQuality,
    AverageBitrate,
    ConstantBitrate,
    LosslessCompression,
};

struct EncoderProfile
{
    QString id;
    QString name;
    QString extension;
    QString containerName;
    QString codecName;
    EncoderMode mode{EncoderMode::Default};
    int bitrateKbps{0};
    double quality{0.0};
    int compressionLevel{-1};
};

struct AudioEncoderSettings
{
    EncoderProfile profile;
    SampleFormat outputSampleFormat{SampleFormat::Unknown};
    DitherMode ditherMode{DitherMode::Never};
};

enum class DestinationMode : uint8_t
{
    Ask = 0,
    SourceFolder,
    FixedFolder,
};

enum class ExistingFileMode : uint8_t
{
    Ask = 0,
    Skip,
    Overwrite,
};

enum class OutputStyle : uint8_t
{
    IndividualFiles = 0,
    MultiTrackFiles,
    MergeTracks,
};

enum class ConversionReplayGainMode : uint8_t
{
    None = 0,
    Track,
    Album,
};

struct ConversionDestination
{
    DestinationMode mode{DestinationMode::Ask};
    QString fixedFolder;
    QString filenamePattern{QStringLiteral("%title%")};
    ExistingFileMode existingFileMode{ExistingFileMode::Ask};
    OutputStyle outputStyle{OutputStyle::IndividualFiles};
};

struct ConversionProcessing
{
    bool transferMetadata{true};
    bool transferRating{true};
    bool transferPlaycount{false};
    bool transferPictures{true};
    ConversionReplayGainMode replayGainMode{ConversionReplayGainMode::None};
    bool replayGainPreventClipping{true};
    double replayGainPreampDb{0.0};
    double replayGainWithoutInfoPreampDb{0.0};
    Engine::DspChain dspChain;
    bool preserveDspStateBetweenTracks{false};
};

struct ConversionPreview
{
    bool enabled{false};
    int lengthPercentage{20};
};

struct ConversionOther
{
    QString copyFilesPattern;
    bool verifyOutput{false};
};

struct ConversionPreset
{
    QString id;
    QString name;
    AudioEncoderSettings encoder;
    ConversionDestination destination;
    ConversionProcessing processing;
    ConversionPreview preview;
    ConversionOther other;
};

struct ConversionJob
{
    TrackList tracks;
    ConversionPreset preset;
};
} // namespace Fooyin
