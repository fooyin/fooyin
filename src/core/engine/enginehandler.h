/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/enginecontroller.h>

#include <QObject>

namespace Fooyin {
class AudioLoader;
class EngineHandlerPrivate;
class PlayerController;
class SettingsManager;

using OutputNames = std::vector<QString>;

class EngineHandler : public EngineController
{
    Q_OBJECT

public:
    explicit EngineHandler(std::shared_ptr<AudioLoader> decoderProvider, PlayerController* playerController,
                           SettingsManager* settings, QObject* parent = nullptr);
    ~EngineHandler() override;

    void setup();
    void prepareNextTrack(const Track& track);

    [[nodiscard]] AudioEngine::PlaybackState engineState() const override;

    [[nodiscard]] OutputNames getAllOutputs() const override;
    [[nodiscard]] OutputDevices getOutputDevices(const QString& output) const override;
    void addOutput(const QString& name, OutputCreator output) override;

private:
    std::unique_ptr<EngineHandlerPrivate> p;
};
} // namespace Fooyin
