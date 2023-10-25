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

#include "fycore_export.h"

#include <core/engine/audiooutput.h>

#include <QObject>

namespace Fy {
namespace Utils {
class SettingsManager;
}
namespace Core {
namespace Player {
class PlayerManager;
}

namespace Engine {
class AudioOutput;
struct AudioOutputBuilder;

using OutputNames = std::vector<QString>;

class FYCORE_EXPORT EngineHandler : public QObject
{
    Q_OBJECT

public:
    explicit EngineHandler(Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                           QObject* parent = nullptr);
    ~EngineHandler() override;

    void setup();
    void shutdown();

    [[nodiscard]] OutputNames getAllOutputs() const;
    [[nodiscard]] OutputDevices getOutputDevices(const QString& output) const;

    void addOutput(const AudioOutputBuilder& output);

signals:
    void outputChanged(Engine::AudioOutput* output);
    void deviceChanged(const QString& device);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Engine
} // namespace Core
} // namespace Fy
