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

class VgmInput : public AudioInput
{
public:
    VgmInput();

    void configurePlayer(PlayerA* player) const;

    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] bool canReadCover() const override;
    [[nodiscard]] bool canWriteMetaData() const override;
    [[nodiscard]] bool isSeekable() const override;

    bool init(const QString& source) override;
    void start() override;
    void stop() override;

    void seek(uint64_t pos) override;

    AudioBuffer readBuffer(size_t bytes) override;

    [[nodiscard]] bool readMetaData(Track& track) override;

    [[nodiscard]] AudioFormat format() const override;
    [[nodiscard]] Error error() const override;

private:
    FySettings m_settings;
    AudioFormat m_format;
    DataLoaderPtr m_loader;
    std::unique_ptr<PlayerA> m_mainPlayer;
};
} // namespace Fooyin::VgmInput
