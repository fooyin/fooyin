/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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
#include <core/engine/enginedefs.h>

#include <QMetaMethod>
#include <QMetaObject>
#include <QObject>
#include <QThread>

#include <map>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Fooyin {
class AudioEngine;
class AudioLoader;
class DspRegistry;
class PlayerController;
class SettingsManager;

using OutputNames = std::vector<QString>;
template <typename Method>
concept EngineCommandMethod = std::is_member_function_pointer_v<Method>;

/*!
 * Bridge between PlayerController-facing actions and AudioEngine internals.
 *
 * Responsibilities:
 * - register/select output backends and devices,
 * - forward playback operations and DSP chain updates,
 * - fan out engine state/errors through EngineController signals.
 */
class EngineHandler : public EngineController
{
    Q_OBJECT

public:
    explicit EngineHandler(std::shared_ptr<AudioLoader> audioLoader, PlayerController* playerController,
                           SettingsManager* settings, DspRegistry* dspRegistry, QObject* parent = nullptr);
    ~EngineHandler() override;

    //! Initialise outputs/settings wiring and connect engine signal relays.
    void setup();

    //! Queue next-track prep and return request id + immediate readiness snapshot.
    [[nodiscard]] Engine::NextTrackPrepareRequest prepareNextTrackForPlayback(const Track& track);

    //! Get current engine playback state
    [[nodiscard]] Engine::PlaybackState engineState() const override;

    //! Get all registered output names
    [[nodiscard]] OutputNames getAllOutputs() const override;

    //! Get devices for a specific output
    [[nodiscard]] OutputDevices getOutputDevices(const QString& output) const override;

    //! Register an audio output
    void addOutput(const QString& name, OutputCreator output) override;

    void setDspChain(const Engine::DspChains& chain);

protected:
    void connectNotify(const QMetaMethod& signal) override;
    void disconnectNotify(const QMetaMethod& signal) override;

private:
    struct CurrentOutput
    {
        QString name;
        QString device;
    };

    template <EngineCommandMethod Method, typename... Args>
    void dispatchCommand(Method method, Args&&... args) const
    {
        auto params = std::make_tuple(std::decay_t<Args>(std::forward<Args>(args))...);

        QMetaObject::invokeMethod(
            m_engine,
            [engine = m_engine, method, params = std::move(params)]() mutable {
                std::apply(
                    [engine, method](auto&&... unpackedArgs) {
                        (engine->*method)(std::forward<decltype(unpackedArgs)>(unpackedArgs)...);
                    },
                    std::move(params));
            },
            Qt::QueuedConnection);
    }

    void publishEvent(const Track& track, bool ready, uint64_t requestId);
    void handleStateChange(Engine::PlaybackState state);
    void handleTrackChange(const Track& track);
    void handleTrackStatus(Engine::TrackStatus status, const Track& track, uint64_t generation);

    void requestPlay() const;
    void requestPause() const;
    void requestStop() const;

    [[nodiscard]] uint64_t nextPrepareRequestId();
    [[nodiscard]] Engine::NextTrackPrepareRequest requestPrepareNextTrack(const Track& track);

    void changeOutput(const QString& output);
    void updateVolume(double volume);
    void updatePosition(uint64_t ms) const;
    void updateLevelReadyRelay();
    void handleNextTrackReadiness(const Track& track, bool ready, uint64_t requestId);
    [[nodiscard]] bool cachedNextTrackReadyFor(const Track& track) const;
    void clearNextTrackReadiness();

    PlayerController* m_playerController;
    SettingsManager* m_settings;

    QThread m_engineThread;
    AudioEngine* m_engine;

    std::map<QString, OutputCreator> m_outputs;
    CurrentOutput m_currentOutput;
    QMetaObject::Connection m_levelReadyRelayConnection;
    bool m_levelReadyRelayConnected;

    Track m_lastPreparedNextTrack;
    bool m_lastPreparedNextTrackReady;
    uint64_t m_nextPrepareTrackRequestId;
};
} // namespace Fooyin
