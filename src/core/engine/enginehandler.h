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
class SettingsManager;
class PlayerManager;
struct AudioOutputBuilder;

using OutputNames = std::vector<QString>;

class EngineHandler : public EngineController
{
    Q_OBJECT

public:
    explicit EngineHandler(PlayerManager* playerManager, SettingsManager* settings, QObject* parent = nullptr);
    ~EngineHandler() override;

    void setup();

    [[nodiscard]] OutputNames getAllOutputs() const override;
    [[nodiscard]] OutputDevices getOutputDevices(const QString& output) const override;
    void addOutput(const AudioOutputBuilder& output) override;

    std::unique_ptr<AudioDecoder> createDecoder() override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
