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

#include "audioplayer.h"
#include "core/models/track.h"

namespace Fy::Core::Engine {
EngineHandler::EngineHandler(Player::PlayerManager* playerManager, QObject* parent)
    : QObject{parent}
    , m_playerManager{playerManager}
    , m_engineThread{new QThread(this)}
    , m_engine{new AudioPlayer()}
{
    m_engine->moveToThread(m_engineThread);
    m_engineThread->start();

    setup();
}

EngineHandler::~EngineHandler()
{
    emit shutdown();
    m_engineThread->quit();
    m_engineThread->wait();
}

void EngineHandler::setup()
{
    connect(m_playerManager, &Player::PlayerManager::playStateChanged, this, &EngineHandler::playStateChanged);
    connect(m_playerManager, &Player::PlayerManager::volumeChanged, m_engine, &AudioPlayer::setVolume);
    connect(m_playerManager, &Player::PlayerManager::currentTrackChanged, m_engine, &AudioPlayer::changeTrack);
    connect(m_engine, &AudioPlayer::positionChanged, m_playerManager, &Player::PlayerManager::setCurrentPosition);
    connect(m_engine, &AudioPlayer::trackFinished, m_playerManager, &Player::PlayerManager::next);
    connect(m_playerManager, &Player::PlayerManager::positionMoved, m_engine, &AudioPlayer::seek);

    connect(this, &EngineHandler::init, m_engine, &AudioPlayer::init);
    connect(this, &EngineHandler::shutdown, m_engine, &Utils::Worker::stopThread);
    connect(this, &EngineHandler::shutdown, m_engine, &AudioPlayer::deleteLater);
    connect(this, &EngineHandler::play, m_engine, &AudioPlayer::play);
    connect(this, &EngineHandler::pause, m_engine, &AudioPlayer::pause);
    connect(this, &EngineHandler::stop, m_engine, &AudioPlayer::stop);

    emit init();
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
} // namespace Fy::Core::Engine
