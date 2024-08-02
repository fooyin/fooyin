/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audioinput.h>

#include <QFile>

#include <sndfile.h>

namespace Fooyin::Snd {
class SndFileDecoder : public AudioDecoder
{
public:
    SndFileDecoder();

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool isSeekable() const override;

    std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override;
    void stop() override;

    void seek(uint64_t pos) override;

    AudioBuffer readBuffer(size_t bytes) override;

private:
    QIODevice* m_file;
    AudioFormat m_format;
    SF_VIRTUAL_IO m_vio;
    SNDFILE* m_sndFile;
    sf_count_t m_currentFrame;
};

class SndFileReader : public AudioReader
{
public:
    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool canReadCover() const override;
    [[nodiscard]] bool canWriteMetaData() const override;

    bool readTrack(const AudioSource& source, Track& track) override;
};
} // namespace Fooyin::Snd
