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
#include <core/engine/outputplugin.h>
#include <core/track.h>

#include <core/player/playermanager.h>
#include <utils/settings/settingsmanager.h>

#include <QThread>

using namespace Qt::Literals::StringLiterals;

namespace Fy::Core::Engine {
struct CurrentOutput
{
    QString name;
    std::unique_ptr<AudioOutput> output;
    QString device;
    std::unique_ptr<AudioOutput> prevOutput;
};

struct EngineHandler::Private
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

        QMetaObject::invokeMethod(engine, &AudioEngine::startup);

        QObject::connect(playerManager, &Player::PlayerManager::currentTrackChanged, engine, &AudioEngine::changeTrack);
        QObject::connect(playerManager, &Player::PlayerManager::positionMoved, engine, &AudioEngine::seek);
        QObject::connect(&engineThread, &QThread::finished, engine, &AudioEngine::deleteLater);
        QObject::connect(engine, &AudioEngine::positionChanged, playerManager,
                         &Player::PlayerManager::setCurrentPosition);
        QObject::connect(engine, &AudioEngine::trackFinished, playerManager, &Player::PlayerManager::next);
        QObject::connect(self, &EngineHandler::outputChanged, engine, &AudioEngine::setAudioOutput);
        QObject::connect(self, &EngineHandler::deviceChanged, engine, &AudioEngine::setOutputDevice);

        updateVolume(settings->value<Settings::OutputVolume>());
    }

    void playStateChanged(Player::PlayState state)
    {
        QMetaObject::invokeMethod(
            engine,
            [this, state]() {
                switch(state) {
                    case(Player::PlayState::Playing): {
                        engine->play();
                        break;
                    }
                    case(Player::PlayState::Paused): {
                        engine->pause();
                        break;
                    }
                    case(Player::PlayState::Stopped): {
                        engine->stop();
                        break;
                    }
                }
            },
            Qt::QueuedConnection);
    }

    void changeOutput(const QString& output)
    {
        const QStringList newOutput = output.split(u"|"_s);

        if(newOutput.empty() || newOutput.size() < 2) {
            return;
        }

        const QString& newName = newOutput.at(0);
        const QString& device  = newOutput.at(1);

        if(outputs.empty()) {
            qWarning() << "No Outputs have been registered";
            return;
        }

        const auto changeCurrentOutput = [this, newName, device]() {
            currentOutput = {newName, outputs.at(newName)(), device, std::move(currentOutput.output)};
            currentOutput.output->setDevice(device);
            emit self->outputChanged(currentOutput.output.get());
        };

        if(!currentOutput.output) {
            changeCurrentOutput();
        }
        else if(currentOutput.name != newName) {
            if(!outputs.contains(newName)) {
                qDebug() << "Output not found: " << newName;
                return;
            }
            changeCurrentOutput();
        }
        else if(currentOutput.device != device) {
            currentOutput.device = device;
            emit self->deviceChanged(device);
        }
    }

    void updateVolume(double volume)
    {
        QMetaObject::invokeMethod(
            engine, [this, volume]() { engine->setVolume(volume); }, Qt::QueuedConnection);
    }
};

EngineHandler::EngineHandler(Player::PlayerManager* playerManager, Utils::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}
{
    QObject::connect(playerManager, &Player::PlayerManager::playStateChanged, this,
                     [this](Player::PlayState state) { p->playStateChanged(state); });

    p->settings->subscribe<Settings::AudioOutput>(this, [this](const QString& output) { p->changeOutput(output); });
    p->settings->subscribe<Settings::OutputVolume>(this, [this](double volume) { p->updateVolume(volume); });
}

EngineHandler::~EngineHandler() = default;

void EngineHandler::setup()
{
    p->changeOutput(p->settings->value<Settings::AudioOutput>());
}

void EngineHandler::shutdown()
{
    QMetaObject::invokeMethod(p->engine, &AudioEngine::shutdown);

    p->engineThread.quit();
    p->engineThread.wait();
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

void EngineHandler::addOutput(const AudioOutputBuilder& output)
{
    if(p->outputs.contains(output.name)) {
        qDebug() << "Output '" + output.name + "' already registered";
        return;
    }
    p->outputs.emplace(output.name, output.creator);
}
} // namespace Fy::Core::Engine

#include "core/engine/moc_enginehandler.cpp"
