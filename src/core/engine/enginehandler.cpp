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

#include "enginehandler.h"

#include "audioengine.h"
#include "audiooutput.h"
#include "core/coresettings.h"
#include "core/engine/ffmpeg/ffmpegengine.h"
#include "core/models/track.h"

#include <utils/settings/settingsmanager.h>

namespace Fy::Core::Engine {
EngineHandler::EngineHandler(Player::PlayerManager* playerManager, Utils::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_playerManager{playerManager}
    , m_settings{settings}
    , m_engineThread{new QThread(this)}
    , m_engine(new FFmpeg::FFmpegEngine())
    , m_output{nullptr}
{
    m_engine->moveToThread(m_engineThread);
    m_engineThread->start();

    connect(m_playerManager, &Player::PlayerManager::playStateChanged, this, &EngineHandler::playStateChanged);
    connect(m_playerManager, &Player::PlayerManager::volumeChanged, m_engine, &AudioEngine::setVolume);
    connect(m_playerManager, &Player::PlayerManager::currentTrackChanged, m_engine, &AudioEngine::changeTrack);
    connect(m_engine, &AudioEngine::positionChanged, m_playerManager, &Player::PlayerManager::setCurrentPosition);
    connect(m_engine, &AudioEngine::trackFinished, m_playerManager, &Player::PlayerManager::next);
    connect(m_playerManager, &Player::PlayerManager::positionMoved, m_engine, &AudioEngine::seek);

    connect(this, &EngineHandler::outputChanged, m_engine, &AudioEngine::setAudioOutput);
    //    connect(this, &EngineHandler::deviceChanged, m_engine, &AudioEngine::setOutputDevice);
    connect(this, &EngineHandler::shutdown, m_engine, &AudioEngine::shutdown);
    connect(this, &EngineHandler::shutdown, m_engine, &QObject::deleteLater);
    connect(this, &EngineHandler::play, m_engine, &AudioEngine::play);
    connect(this, &EngineHandler::pause, m_engine, &AudioEngine::pause);
    connect(this, &EngineHandler::stop, m_engine, &AudioEngine::stop);
}

EngineHandler::~EngineHandler()
{
    emit shutdown();
    m_engineThread->quit();
    m_engineThread->wait();
}

void EngineHandler::setup()
{
    m_settings->subscribe<Settings::AudioOutput>(this, &EngineHandler::changeOutput);
    m_settings->subscribe<Settings::OutputDevice>(this, &EngineHandler::changeOutputDevice);

    changeOutput(m_settings->value<Settings::AudioOutput>());
    changeOutputDevice(m_settings->value<Settings::OutputDevice>());
}

OutputNames EngineHandler::getAllOutputs() const
{
    OutputNames outputs;

    for(const auto& [name, output] : m_outputs) {
        outputs.emplace_back(name);
    }

    return outputs;
}

std::optional<OutputDevices> EngineHandler::getOutputDevices(const QString& output) const
{
    if(!m_outputs.count(output)) {
        qDebug() << "Output not found: " << output;
        return {};
    }
    const auto devices = m_outputs.at(output)->getAllDevices();
    return devices;
}

void EngineHandler::addOutput(std::unique_ptr<AudioOutput> output)
{
    m_outputs.emplace(output->name(), std::move(output));
}

void EngineHandler::playStateChanged(Player::PlayState state)
{
    switch(state) {
        case(Player::Playing):
            return emit play();
        case(Player::Paused):
            return emit pause();
        case(Player::Stopped):
            return emit stop();
        default:
            return;
    }
}

void EngineHandler::changeOutput(const QString& output)
{
    if(m_outputs.empty()) {
        qWarning() << "No Outputs have been registered";
        return;
    }

    if(m_output && m_output->name() == output) {
        return;
    }

    if(!m_outputs.count(output)) {
        qDebug() << "Output not found: " << output;
        m_output = m_outputs.cbegin()->second.get();
    }
    else {
        m_output = m_outputs.at(output).get();
    }

    qDebug() << "Output changed: " << m_output->name();

    emit outputChanged(m_output);
}

void EngineHandler::changeOutputDevice(const QString& device)
{
    if(m_outputs.empty()) {
        qWarning() << "No Outputs have been registered";
        return;
    }
    emit deviceChanged(device);
}
} // namespace Fy::Core::Engine
