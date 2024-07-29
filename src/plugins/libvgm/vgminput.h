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

#include <QStringList>

#include <player/playera.hpp>

namespace Fooyin::VgmInput {
struct DataLoaderDeleter
{
    void operator()(DATA_LOADER* loader) const noexcept
    {
        if(loader) {
            DataLoader_Deinit(loader);
        }
    }
};
using DataLoaderPtr = std::unique_ptr<DATA_LOADER, DataLoaderDeleter>;

class VgmDecoder : public AudioDecoder
{
public:
    VgmDecoder();

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool isSeekable() const override;

    std::optional<AudioFormat> init(const Track& track, DecoderOptions options) override;
    void start() override;
    void stop() override;

    void seek(uint64_t pos) override;

    AudioBuffer readBuffer(size_t bytes) override;

private:
    FySettings m_settings;
    AudioFormat m_format;
    DataLoaderPtr m_loader;
    std::unique_ptr<PlayerA> m_mainPlayer;
};

class VgmReader : public AudioReader
{
public:
    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool canReadCover() const override;
    [[nodiscard]] bool canWriteMetaData() const override;

    bool readTrack(Track& track) override;
};
} // namespace Fooyin::VgmInput
