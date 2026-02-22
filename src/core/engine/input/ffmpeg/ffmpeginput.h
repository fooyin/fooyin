/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audioinput.h>

#include "ffmpegframe.h"

struct AVFormatContext;

namespace Fooyin {
class AudioFormat;
class AudioBuffer;
class FFmpegInputPrivate;

class FYCORE_EXPORT FFmpegDecoder : public AudioDecoder
{
public:
    FFmpegDecoder();
    ~FFmpegDecoder() override;

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] int bitrate() const override;

    std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override;
    void start() override;
    void stop() override;

    [[nodiscard]] bool isSeekable() const override;
    void seek(uint64_t pos) override;

    Frame readFrame();
    AudioBuffer readBuffer(size_t bytes) override;

    [[nodiscard]] std::optional<bool> isPlanar() const;

private:
    std::unique_ptr<FFmpegInputPrivate> p;
};

class FFmpegReader : public AudioReader
{
public:
    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool canReadCover() const override;
    [[nodiscard]] bool canWriteMetaData() const override;

    [[nodiscard]] bool readTrack(const AudioSource& source, Track& track) override;
    [[nodiscard]] QByteArray readCover(const AudioSource& source, const Track& track, Track::Cover cover) override;
    [[nodiscard]] bool writeTrack(const AudioSource& source, const Track& track, WriteOptions options) override;
};
} // namespace Fooyin
