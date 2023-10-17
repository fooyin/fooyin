/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audiooutput.h>

#include <SDL2/SDL_audio.h>

#include <QString>

namespace Fy::Core::Engine {
class SdlOutput : public AudioPullOutput
{
public:
    SdlOutput();
    ~SdlOutput() override;

    bool init(const OutputContext& oc) override;
    void uninit() override;
    void reset() override;
    void start() override;

    [[nodiscard]] bool initialised() const override;
    [[nodiscard]] QString device() const override;
    [[nodiscard]] bool canHandleVolume() const override;
    [[nodiscard]] OutputDevices getAllDevices() const override;

    void setPaused(bool pause) override;
    void setDevice(const QString& device) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fy::Core::Engine
