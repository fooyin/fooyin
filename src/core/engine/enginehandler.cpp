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

#include "core/models/track.h"

namespace Core::Engine {
EngineHandler::EngineHandler(Player::PlayerManager* playerManager, QObject* parent)
    : Worker(parent)
{
    connect(playerManager, &Player::PlayerManager::playStateChanged, this, &EngineHandler::playStateChanged);
    connect(playerManager, &Player::PlayerManager::volumeChanged, &m_engine, &Engine::setVolume);
    connect(playerManager, &Player::PlayerManager::currentTrackChanged, &m_engine, &Engine::changeTrack);
    connect(&m_engine, &Engine::currentPositionChanged, playerManager, &Player::PlayerManager::setCurrentPosition);
    connect(&m_engine, &Engine::trackFinished, playerManager, &Player::PlayerManager::next);
    connect(playerManager, &Player::PlayerManager::positionMoved, &m_engine, &Engine::seek);

    connect(this, &EngineHandler::play, &m_engine, &Engine::play);
    connect(this, &EngineHandler::pause, &m_engine, &Engine::pause);
    connect(this, &EngineHandler::stop, &m_engine, &Engine::stop);
}

EngineHandler::~EngineHandler() = default;

void EngineHandler::playStateChanged(Player::PlayState state)
{
    switch(state) {
        case Player::PlayState::Playing:
            return emit play();
        case Player::PlayState::Paused:
            return emit pause();
        case Player::PlayState::Stopped:
            return emit stop();
        default:
            return;
    }
}
} // namespace Core::Engine
