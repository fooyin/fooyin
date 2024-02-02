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

#include "fycore_export.h"

#include <core/engine/audioengine.h>
#include <core/engine/audiooutput.h>

#include <QObject>

namespace Fooyin {
struct AudioOutputBuilder;
class AudioDecoder;

using OutputNames = std::vector<QString>;

class FYCORE_EXPORT EngineController : public QObject
{
    Q_OBJECT

public:
    explicit EngineController(QObject* parent = nullptr)
        : QObject{parent}
    { }

    /** Returns a list of all output names. */
    [[nodiscard]] virtual OutputNames getAllOutputs() const = 0;

    /** Returns a list of all output devices for the given @p output. */
    [[nodiscard]] virtual OutputDevices getOutputDevices(const QString& output) const = 0;

    /*!
     * Adds an audio output.
     * @note output.name must be unique.
     */
    virtual void addOutput(const AudioOutputBuilder& output) = 0;

    virtual std::unique_ptr<AudioDecoder> createDecoder() = 0;

signals:
    void outputChanged(AudioOutput* output);
    void deviceChanged(const QString& device);
    void trackStatusChanged(TrackStatus status);
};
} // namespace Fooyin
