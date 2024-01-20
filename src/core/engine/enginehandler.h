/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "audioengine.h"

#include <core/engine/audiooutput.h>

#include <QObject>

namespace Fooyin {
class SettingsManager;
class PlayerManager;
struct AudioOutputBuilder;

using OutputNames = std::vector<QString>;

class EngineHandler : public QObject
{
    Q_OBJECT

public:
    explicit EngineHandler(PlayerManager* playerManager, SettingsManager* settings, QObject* parent = nullptr);
    ~EngineHandler() override;

    void setup();
    void shutdown();

    /** Returns a list of all output names. */
    [[nodiscard]] OutputNames getAllOutputs() const;

    /** Returns a list of all output devices for the given @p output. */
    [[nodiscard]] OutputDevices getOutputDevices(const QString& output) const;

    /*!
     * Adds an audio output.
     * @note output.name must be unique.
     */
    void addOutput(const AudioOutputBuilder& output);

signals:
    void trackStatusChanged(TrackStatus status);
    void outputChanged(AudioOutput* output);
    void deviceChanged(const QString& device);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
