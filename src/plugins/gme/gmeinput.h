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

#include <core/coresettings.h>
#include <core/engine/audioinput.h>

#include <gme/gme.h>

namespace Fooyin::Gme {
struct MusicEmuDeleter
{
    void operator()(Music_Emu* emu) const
    {
        if(emu) {
            gme_delete(emu);
        }
    }
};
using MusicEmuPtr = std::unique_ptr<Music_Emu, MusicEmuDeleter>;

class GmeDecoder : public AudioDecoder
{
public:
    GmeDecoder();

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool isSeekable() const override;
    [[nodiscard]] bool trackHasChanged() const override;
    [[nodiscard]] Track changedTrack() const override;

    std::optional<AudioFormat> init(const Track& track, DecoderOptions options) override;
    void start() override;
    void stop() override;

    void seek(uint64_t pos) override;

    AudioBuffer readBuffer(size_t bytes) override;

private:
    DecoderOptions m_options;
    FySettings m_settings;
    AudioFormat m_format;
    MusicEmuPtr m_emu;
    int m_subsong;
    int m_duration;
    int m_loopLength;
    Track m_changedTrack;
};

class GmeReader : public AudioReader
{
public:
    GmeReader();

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool canReadCover() const override;
    [[nodiscard]] bool canWriteMetaData() const override;
    [[nodiscard]] int subsongCount() const override;

    bool init(const QString& source) override;
    bool readTrack(Track& track) override;

private:
    MusicEmuPtr m_emu;
    int m_subsongCount;
};
} // namespace Fooyin::Gme
