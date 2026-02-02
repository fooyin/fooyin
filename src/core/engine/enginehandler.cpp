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

#include "enginehandler.h"

#include "audioengine.h"
#include "enginehelpers.h"
#include <core/coresettings.h>
#include <core/engine/enginedefs.h>
#include <core/player/playercontroller.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

#include <QLoggingCategory>

#include <utility>

Q_LOGGING_CATEGORY(ENG_HANDLER, "fy.engine")

using namespace Qt::StringLiterals;

namespace Fooyin {
EngineHandler::EngineHandler(std::shared_ptr<AudioLoader> audioLoader, PlayerController* playerController,
                             SettingsManager* settings, DspRegistry* dspRegistry, QObject* parent)
    : EngineController{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_engine{new AudioEngine(std::move(audioLoader), settings, dspRegistry)}
    , m_levelReadyRelayConnected{false}
    , m_lastPreparedNextTrackReady{false}
    , m_nextPrepareTrackRequestId{1}
{
    m_engine->moveToThread(&m_engineThread);

    QObject::connect(m_playerController, &PlayerController::positionMoved, this,
                     [this](uint64_t positionMs) { dispatchCommand(&AudioEngine::seek, positionMs); });

    QObject::connect(m_engine, &AudioEngine::trackAboutToFinish, this, [this](const Track& track, uint64_t generation) {
        emit trackAboutToFinish(Engine::AboutToFinishContext{track, generation});
    });
    QObject::connect(m_engine, &AudioEngine::trackReadyToSwitch, this, [this](const Track& track, uint64_t generation) {
        emit trackReadyToSwitch(Engine::AboutToFinishContext{track, generation});
    });
    QObject::connect(m_engine, &AudioEngine::finished, this, &EngineHandler::finished);
    QObject::connect(m_engine, &AudioEngine::positionChanged, this, [this](uint64_t ms) { updatePosition(ms); });
    QObject::connect(m_engine, &AudioEngine::bitrateChanged, m_playerController, &PlayerController::setBitrate);
    QObject::connect(m_engine, &AudioEngine::stateChanged, this,
                     [this](Engine::PlaybackState state) { handleStateChange(state); });
    QObject::connect(m_engine, &AudioEngine::deviceError, this, &EngineController::engineError);
    QObject::connect(m_engine, &AudioEngine::trackChanged, this, &EngineController::trackChanged);
    QObject::connect(m_engine, &AudioEngine::trackStatusContextChanged, this,
                     [this](Engine::TrackStatus status, const Track& track, uint64_t generation) {
                         handleTrackStatus(status, track, generation);
                     });
    QObject::connect(m_engine, &AudioEngine::nextTrackReadiness, this,
                     [this](const Track& track, bool ready, uint64_t requestId) {
                         handleNextTrackReadiness(track, ready, requestId);
                     });

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                     [this]() { handleTrackChange(m_playerController->currentTrack()); });

    m_engineThread.start();
    updateLevelReadyRelay();

    updateVolume(m_settings->value<Settings::Core::OutputVolume>());

    QObject::connect(m_playerController, &PlayerController::transportPlayRequested, this, [this]() { requestPlay(); });
    QObject::connect(m_playerController, &PlayerController::transportPauseRequested, this,
                     [this]() { requestPause(); });
    QObject::connect(m_playerController, &PlayerController::transportStopRequested, this, [this]() { requestStop(); });

    QObject::connect(this, &EngineHandler::outputChanged, this, [this](const QString& output, const QString& device) {
        if(m_outputs.contains(output)) {
            dispatchCommand(&AudioEngine::setAudioOutput, m_outputs.at(output), device);
        }
    });
    QObject::connect(this, &EngineHandler::deviceChanged, this,
                     [this](const QString& device) { dispatchCommand(&AudioEngine::setOutputDevice, device); });

    m_settings->subscribe<Settings::Core::AudioOutput>(this, [this](const QString& output) { changeOutput(output); });
    m_settings->subscribe<Settings::Core::OutputVolume>(this, [this](double volume) { updateVolume(volume); });
}

EngineHandler::~EngineHandler()
{
    m_engine->deleteLater();
    m_engineThread.quit();
    m_engineThread.wait();
}

void EngineHandler::connectNotify(const QMetaMethod& signal)
{
    EngineController::connectNotify(signal);

    if(signal == QMetaMethod::fromSignal(&EngineController::levelReady)) {
        QMetaObject::invokeMethod(this, &EngineHandler::updateLevelReadyRelay, Qt::QueuedConnection);
    }
}

void EngineHandler::disconnectNotify(const QMetaMethod& signal)
{
    EngineController::disconnectNotify(signal);

    if(signal == QMetaMethod::fromSignal(&EngineController::levelReady)) {
        QMetaObject::invokeMethod(this, &EngineHandler::updateLevelReadyRelay, Qt::QueuedConnection);
    }
}

void EngineHandler::publishEvent(const Track& track, bool ready, uint64_t requestId)
{
    m_lastPreparedNextTrack      = track;
    m_lastPreparedNextTrackReady = ready;
    emit nextTrackReadiness(track, ready, requestId);
}

uint64_t EngineHandler::nextPrepareRequestId()
{
    return m_nextPrepareTrackRequestId++;
}

Engine::NextTrackPrepareRequest EngineHandler::requestPrepareNextTrack(const Track& track)
{
    const uint64_t requestId = nextPrepareRequestId();
    const bool readyNow      = cachedNextTrackReadyFor(track);

    dispatchCommand(&AudioEngine::prepareNextTrack, track, requestId);

    return Engine::NextTrackPrepareRequest{
        .requestId = requestId,
        .readyNow  = readyNow,
    };
}

void EngineHandler::handleStateChange(Engine::PlaybackState state)
{
    switch(state) {
        case Engine::PlaybackState::Error:
        case Engine::PlaybackState::Stopped:
            m_playerController->syncPlayStateFromEngine(Player::PlayState::Stopped);
            break;
        case Engine::PlaybackState::Paused:
            m_playerController->syncPlayStateFromEngine(Player::PlayState::Paused);
            break;
        case Engine::PlaybackState::Playing:
            m_playerController->syncPlayStateFromEngine(Player::PlayState::Playing);
            break;
    }

    emit engineStateChanged(state);
}

void EngineHandler::handleTrackChange(const Track& track)
{
    if(!track.isValid()) {
        return;
    }
    clearNextTrackReadiness();

    const bool manualChange = m_playerController->lastTrackChangeContext().userInitiated;

    dispatchCommand(&AudioEngine::loadTrack, track, manualChange);
}

void EngineHandler::handleTrackStatus(Engine::TrackStatus status, const Track& track, uint64_t generation)
{
    switch(status) {
        case Engine::TrackStatus::NoTrack:
            clearNextTrackReadiness();
            m_playerController->syncPlayStateFromEngine(Player::PlayState::Stopped);
            break;
        case Engine::TrackStatus::End:
        case Engine::TrackStatus::Invalid:
        case Engine::TrackStatus::Loading:
        case Engine::TrackStatus::Loaded:
        case Engine::TrackStatus::Buffered:
        case Engine::TrackStatus::Unreadable:
            break;
    }

    emit trackStatusContextChanged(Engine::TrackStatusContext{status, track, generation});
}

void EngineHandler::requestPlay() const
{
    dispatchCommand(&AudioEngine::play);
}

void EngineHandler::requestPause() const
{
    dispatchCommand(&AudioEngine::pause);
}

void EngineHandler::requestStop() const
{
    dispatchCommand(&AudioEngine::stop);
}

void EngineHandler::updateLevelReadyRelay()
{
    const bool hasSubscribers = isSignalConnected(QMetaMethod::fromSignal(&EngineController::levelReady));
    Engine::AnalysisDataTypes subscriptions;

    if(hasSubscribers) {
        subscriptions.setFlag(Engine::AnalysisDataType::LevelFrameData);
    }

    if(hasSubscribers && !m_levelReadyRelayConnected) {
        m_levelReadyRelayConnection
            = QObject::connect(m_engine, &AudioEngine::levelReady, this, &EngineController::levelReady);
        m_levelReadyRelayConnected = true;
    }
    else if(!hasSubscribers && m_levelReadyRelayConnected) {
        QObject::disconnect(m_levelReadyRelayConnection);
        m_levelReadyRelayConnection = {};
        m_levelReadyRelayConnected  = false;
    }

    dispatchCommand(&AudioEngine::setAnalysisDataSubscriptions, subscriptions);
}

void EngineHandler::changeOutput(const QString& output)
{
    auto loadDefault = [this]() {
        if(m_outputs.empty()) {
            return;
        }
        m_currentOutput = {.name = m_outputs.cbegin()->first, .device = u"default"_s};
        emit outputChanged(m_currentOutput.name, m_currentOutput.device);
    };

    if(output.isEmpty()) {
        if(m_outputs.empty() || !m_currentOutput.name.isEmpty()) {
            return;
        }
        loadDefault();
        return;
    }

    const auto newOutput = output.split(u"|"_s);

    if(newOutput.empty() || newOutput.size() < 2) {
        return;
    }

    const QString& newName = newOutput.at(0);
    const QString& device  = newOutput.at(1);

    if(m_outputs.empty()) {
        qCWarning(ENG_HANDLER) << "No Outputs have been registered";
        return;
    }

    if(!m_outputs.contains(newName)) {
        qCWarning(ENG_HANDLER) << "Output hasn't been registered:" << newName;
        loadDefault();
        return;
    }

    if(m_currentOutput.name != newName) {
        m_currentOutput = {.name = newName, .device = device};
        emit outputChanged(newName, device);
    }
    else if(m_currentOutput.device != device) {
        m_currentOutput.device = device;
        emit deviceChanged(device);
    }
}

void EngineHandler::updateVolume(double volume)
{
    dispatchCommand(&AudioEngine::setVolume, volume);
}

void EngineHandler::updatePosition(uint64_t ms) const
{
    m_playerController->setCurrentPosition(ms);
}

void EngineHandler::handleNextTrackReadiness(const Track& track, bool ready, uint64_t requestId)
{
    publishEvent(track, ready, requestId);
}

bool EngineHandler::cachedNextTrackReadyFor(const Track& track) const
{
    return m_lastPreparedNextTrackReady && sameTrackIdentity(m_lastPreparedNextTrack, track);
}

void EngineHandler::clearNextTrackReadiness()
{
    m_lastPreparedNextTrack      = {};
    m_lastPreparedNextTrackReady = false;
}

void EngineHandler::setup()
{
    changeOutput(m_settings->value<Settings::Core::AudioOutput>());
}

void EngineHandler::setDspChain(const Engine::DspChains& chain)
{
    dispatchCommand(&AudioEngine::setDspChain, chain);
}

Engine::NextTrackPrepareRequest EngineHandler::prepareNextTrackForPlayback(const Track& track)
{
    return requestPrepareNextTrack(track);
}

Engine::PlaybackState EngineHandler::engineState() const
{
    return m_engine->playbackState();
}

OutputNames EngineHandler::getAllOutputs() const
{
    OutputNames outputs;
    outputs.reserve(m_outputs.size());

    for(const auto& [name, output] : m_outputs) {
        outputs.emplace_back(name);
    }

    return outputs;
}

OutputDevices EngineHandler::getOutputDevices(const QString& output) const
{
    if(!m_outputs.contains(output)) {
        qCWarning(ENG_HANDLER) << "Output" << output << "not found";
        return {};
    }

    if(auto out = m_outputs.at(output)()) {
        const bool isCurrent
            = m_engine->playbackState() != Engine::PlaybackState::Stopped
           && (m_currentOutput.name == output || m_currentOutput.device.compare(output, Qt::CaseInsensitive) == 0);
        return out->getAllDevices(isCurrent);
    }

    return {};
}

void EngineHandler::addOutput(const QString& name, OutputCreator output)
{
    if(m_outputs.contains(name)) {
        qCWarning(ENG_HANDLER) << "Output" << name << "already registered";
        return;
    }
    m_outputs.emplace(name, std::move(output));
}
} // namespace Fooyin
