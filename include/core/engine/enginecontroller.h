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

#include <core/engine/audiooutput.h>
#include <core/engine/enginedefs.h>
#include <core/engine/levelframe.h>

#include <QByteArray>
#include <QObject>

namespace Fooyin {
struct AudioOutputBuilder;
class AudioBuffer;

using OutputNames = std::vector<QString>;

/*!
 * High-level playback engine facade exposed to app/UI layers.
 *
 * Implementations bridge UI/player requests to an underlying audio engine and
 * emit normalised engine events/signals for the rest of the application.
 */
class FYCORE_EXPORT EngineController : public QObject
{
    Q_OBJECT

public:
    explicit EngineController(QObject* parent = nullptr)
        : QObject{parent}
    { }

    //! Current playback state as reported by the engine.
    [[nodiscard]] virtual Engine::PlaybackState engineState() const = 0;
    //! All registered output backend names.
    [[nodiscard]] virtual OutputNames getAllOutputs() const = 0;
    //! Available devices for a specific output backend.
    [[nodiscard]] virtual OutputDevices getOutputDevices(const QString& output) const = 0;

    /*!
     * Registers an audio output backend factory under @p name.
     * @note @p name must be unique.
     */
    virtual void addOutput(const QString& name, OutputCreator output) = 0;

signals:
    //! Emitted when output backend selection changes.
    void outputChanged(const QString& output, const QString& device);
    //! Emitted when device for current backend changes.
    void deviceChanged(const QString& device);

    //! Fatal/non-recoverable engine error text.
    void engineError(const QString& error);
    //! Playback state transitions.
    void engineStateChanged(Fooyin::Engine::PlaybackState state);
    //! Track-status transitions with stable generation identifier.
    void trackStatusContextChanged(const Fooyin::Engine::TrackStatusContext& context);

    //! Fixed-hop level analysis snapshots.
    void levelReady(const Fooyin::LevelFrame& frame);

    //! Metadata/context update for currently active track.
    void trackChanged(const Fooyin::Track& track);
    //! Early callback before track reaches terminal end.
    void trackAboutToFinish(const Fooyin::Engine::AboutToFinishContext& context);
    //! Callback fired when transition timing reaches the switch anchor.
    void trackReadyToSwitch(const Fooyin::Engine::AboutToFinishContext& context);
    //! Prepared-next-track readiness notification.
    void nextTrackReadiness(const Fooyin::Track& track, bool ready, uint64_t requestId);

    //! Emitted after stop teardown completes.
    void finished();
};
} // namespace Fooyin
