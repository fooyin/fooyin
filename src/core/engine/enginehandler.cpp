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

#include <core/engine/enginehandler.h>

#include "engine/ffmpeg/ffmpegengine.h"

#include <core/coresettings.h>
#include <core/engine/audioengine.h>
#include <core/track.h>

#include <core/player/playermanager.h>
#include <utils/settings/settingsmanager.h>

#include <QThread>

namespace Fy::Core::Engine {
struct CurrentOutput
{
    QString name;
    std::unique_ptr<AudioOutput> output;
    std::unique_ptr<AudioOutput> prevOutput;
};

struct EngineHandler::Private : QObject
{
    EngineHandler* self;

    Player::PlayerManager* playerManager;
    Utils::SettingsManager* settings;

    QThread engineThread;
    AudioEngine* engine;

    std::map<QString, OutputCreator> outputs;
    CurrentOutput currentOutput;

    Private(EngineHandler* self, Player::PlayerManager* playerManager, Utils::SettingsManager* settings)
        : self{self}
        , playerManager{playerManager}
        , settings{settings}
        , engine{new FFmpeg::FFmpegEngine()}
    {
        engine->moveToThread(&engineThread);
        engineThread.start();

        QObject::connect(playerManager, &Player::PlayerManager::playStateChanged, this,
                         &EngineHandler::Private::playStateChanged);
        QObject::connect(playerManager, &Player::PlayerManager::volumeChanged, engine, &AudioEngine::setVolume);
        QObject::connect(playerManager, &Player::PlayerManager::currentTrackChanged, engine, &AudioEngine::changeTrack);
        QObject::connect(engine, &AudioEngine::positionChanged, playerManager,
                         &Player::PlayerManager::setCurrentPosition);
        QObject::connect(engine, &AudioEngine::trackFinished, playerManager, &Player::PlayerManager::next);
        QObject::connect(playerManager, &Player::PlayerManager::positionMoved, engine, &AudioEngine::seek);

        connect(self, &EngineHandler::outputChanged, engine, &AudioEngine::setAudioOutput);
        connect(self, &EngineHandler::deviceChanged, engine, &AudioEngine::setOutputDevice);

        QMetaObject::invokeMethod(engine, [this]() {
            engine->setVolume(this->settings->value<Settings::OutputVolume>());
        });
    }

    void playStateChanged(Player::PlayState state)
    {
        QMetaObject::invokeMethod(engine, [this, state]() {
            switch(state) {
                case(Player::PlayState::Playing):
                    return engine->play();
                case(Player::PlayState::Paused):
                    return engine->pause();
                case(Player::PlayState::Stopped):
                    return engine->stop();
            }
        });
    }
};

EngineHandler::EngineHandler(Player::PlayerManager* playerManager, Utils::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}
{ }

EngineHandler::~EngineHandler()
{
    QMetaObject::invokeMethod(p->engine, [this]() {
        p->engine->shutdown();
        p->engine->deleteLater();
    });

    p->engineThread.quit();
    p->engineThread.wait();
}

void EngineHandler::setup()
{
    p->settings->subscribe<Settings::AudioOutput>(this, &EngineHandler::changeOutput);
    p->settings->subscribe<Settings::OutputDevice>(this, &EngineHandler::changeOutputDevice);

    changeOutput(p->settings->value<Settings::AudioOutput>());
    if(p->currentOutput.output) {
        p->currentOutput.output->setDevice(p->settings->value<Settings::OutputDevice>());
    }
}

OutputNames EngineHandler::getAllOutputs() const
{
    OutputNames outputs;

    for(const auto& [name, output] : p->outputs) {
        outputs.emplace_back(name);
    }

    return outputs;
}

OutputDevices EngineHandler::getOutputDevices(const QString& output) const
{
    if(!p->outputs.contains(output)) {
        qDebug() << "Output not found: " << output;
        return {};
    }

    if(p->currentOutput.name == output) {
        return p->currentOutput.output->getAllDevices();
    }

    if(auto out = p->outputs.at(output)()) {
        return out->getAllDevices();
    }

    return {};
}

void EngineHandler::addOutput(const QString& name, OutputCreator output)
{
    p->outputs.emplace(name, std::move(output));
}

void EngineHandler::changeOutput(const QString& output)
{
    if(p->outputs.empty()) {
        qWarning() << "No Outputs have been registered";
        return;
    }

    if(p->currentOutput.output && p->currentOutput.name == output) {
        return;
    }

    if(!p->outputs.contains(output)) {
        qDebug() << "Output not found: " << output;
        return;
    }

    p->currentOutput = {output, p->outputs.at(output)(), std::move(p->currentOutput.output)};
    qDebug() << "Output changed: " << p->currentOutput.name;

    emit outputChanged(p->currentOutput.output.get());
}

void EngineHandler::changeOutputDevice(const QString& device)
{
    if(p->outputs.empty()) {
        qWarning() << "No Outputs have been registered";
        return;
    }
    emit deviceChanged(device);
}
} // namespace Fy::Core::Engine
