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

#include "enginempv.h"

namespace Fy::Core::Engine {
struct EngineHandler::Private : QObject
{
    EngineHandler* self;

    Engine* engine;

    explicit Private(EngineHandler* self)
        : self{self}
        , engine{new EngineMpv(self)}
    { }

    void playStateChanged(Player::PlayState state)
    {
        switch(state) {
            case Player::PlayState::Playing:
                return emit self->play();
            case Player::PlayState::Paused:
                return emit self->pause();
            case Player::PlayState::Stopped:
                return emit self->stop();
            default:
                return;
        }
    }
};

EngineHandler::EngineHandler(Player::PlayerManager* playerManager, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this)}
{
    connect(playerManager, &Player::PlayerManager::playStateChanged, p.get(),
            &EngineHandler::Private::playStateChanged);
    connect(playerManager, &Player::PlayerManager::volumeChanged, p->engine, &Engine::setVolume);
    connect(playerManager, &Player::PlayerManager::currentTrackChanged, p->engine, &Engine::changeTrack);
    connect(p->engine, &Engine::currentPositionChanged, playerManager, &Player::PlayerManager::setCurrentPosition);
    connect(p->engine, &Engine::trackFinished, playerManager, &Player::PlayerManager::next);
    connect(playerManager, &Player::PlayerManager::positionMoved, p->engine, &Engine::seek);

    connect(this, &EngineHandler::play, p->engine, &Engine::play);
    connect(this, &EngineHandler::pause, p->engine, &Engine::pause);
    connect(this, &EngineHandler::stop, p->engine, &Engine::stop);
}

EngineHandler::~EngineHandler() = default;
} // namespace Fy::Core::Engine
