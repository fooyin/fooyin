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

#include <libopenmpt/libopenmpt.hpp>

namespace Fooyin::OpenMpt {
class OpenMptInput : public AudioDecoder
{
public:
    OpenMptInput();

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool isSeekable() const override;

    std::optional<AudioFormat> init(const Track& source, DecoderOptions options) override;

    void stop() override;
    void seek(uint64_t pos) override;
    AudioBuffer readBuffer(size_t bytes) override;

private:
    std::unique_ptr<openmpt::module> m_module;
    AudioFormat m_format;
    bool m_eof;
};

class OpenMptReader : public AudioReader
{
public:
    OpenMptReader();

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool canReadCover() const override;
    [[nodiscard]] bool canWriteMetaData() const override;
    [[nodiscard]] int subsongCount() const override;

    bool init(const QString& source) override;
    bool readTrack(Track& track) override;

private:
    std::unique_ptr<openmpt::module> m_module;
    int m_subsongCount;
};
} // namespace Fooyin::OpenMpt
