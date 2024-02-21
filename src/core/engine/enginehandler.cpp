/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "engine/ffmpeg/ffmpegdecoder.h"
#include "engine/ffmpeg/ffmpegengine.h"

#include <core/coresettings.h>
#include <core/engine/outputplugin.h>
#include <core/track.h>

#include <core/player/playermanager.h>
#include <utils/settings/settingsmanager.h>

#include <QThread>

namespace Fooyin {
struct CurrentOutput
{
    QString name;
    QString device;
};

struct EngineHandler::Private
{
    EngineHandler* self;

    PlayerManager* playerManager;
    SettingsManager* settings;

    QThread engineThread;
    AudioEngine* engine;

    std::map<QString, OutputCreator> outputs;
    CurrentOutput currentOutput;

    Private(EngineHandler* self_, PlayerManager* playerManager_, SettingsManager* settings_)
        : self{self_}
        , playerManager{playerManager_}
        , settings{settings_}
        , engine{new FFmpegEngine(settings)}
    {
        engine->moveToThread(&engineThread);
        engineThread.start();

        QObject::connect(playerManager, &PlayerManager::currentTrackChanged, engine, &AudioEngine::changeTrack);
        QObject::connect(playerManager, &PlayerManager::positionMoved, engine, &AudioEngine::seek);
        QObject::connect(&engineThread, &QThread::finished, engine, &AudioEngine::deleteLater);
        QObject::connect(engine, &AudioEngine::positionChanged, playerManager, &PlayerManager::setCurrentPosition);
        QObject::connect(engine, &AudioEngine::trackStatusChanged, self,
                         [this](TrackStatus status) { handleTrackStatus(status); });

        updateVolume(settings->value<Settings::Core::OutputVolume>());
    }

    void handleTrackStatus(TrackStatus status) const
    {
        switch(status) {
            case(TrackStatus::EndOfTrack):
                playerManager->next();
                break;
            case(NoTrack):
                playerManager->stop();
                break;
            case(InvalidTrack):
            case(LoadingTrack):
            case(LoadedTrack):
            case(BufferedTrack):
                break;
        }

        QMetaObject::invokeMethod(self, "trackStatusChanged", Q_ARG(TrackStatus, status));
    }

    void playStateChanged(PlayState state)
    {
        QMetaObject::invokeMethod(
            engine,
            [this, state]() {
                switch(state) {
                    case(PlayState::Playing): {
                        engine->play();
                        break;
                    }
                    case(PlayState::Paused): {
                        engine->pause();
                        break;
                    }
                    case(PlayState::Stopped): {
                        engine->stop();
                        break;
                    }
                }
            },
            Qt::QueuedConnection);
    }

    void changeOutput(const QString& output)
    {
        if(output.isEmpty()) {
            return;
        }

        const QStringList newOutput = output.split(QStringLiteral("|"));

        if(newOutput.empty() || newOutput.size() < 2) {
            return;
        }

        const QString& newName = newOutput.at(0);
        const QString& device  = newOutput.at(1);

        if(outputs.empty()) {
            qWarning() << "No Outputs have been registered";
            return;
        }

        if(!outputs.contains(newName)) {
            qWarning() << QString{QStringLiteral("Output (%1) hasn't been registered")}.arg(newName);
            return;
        }

        if(currentOutput.name != newName) {
            currentOutput = {newName, device};
            QMetaObject::invokeMethod(self, "outputChanged", Q_ARG(const QString&, newName));
            QMetaObject::invokeMethod(self, "deviceChanged", Q_ARG(const QString&, device));
        }
        else if(currentOutput.device != device) {
            currentOutput.device = device;
            QMetaObject::invokeMethod(self, "deviceChanged", Q_ARG(const QString&, device));
        }
    }

    void updateVolume(double volume)
    {
        QMetaObject::invokeMethod(
            engine, [this, volume]() { engine->setVolume(volume); }, Qt::QueuedConnection);
    }
};

EngineHandler::EngineHandler(PlayerManager* playerManager, SettingsManager* settings, QObject* parent)
    : EngineController{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}
{
    QObject::connect(playerManager, &PlayerManager::playStateChanged, this,
                     [this](PlayState state) { p->playStateChanged(state); });

    QObject::connect(this, &EngineHandler::outputChanged, this, [this](const QString& output) {
        if(p->outputs.contains(output)) {
            const auto& outputCreator = p->outputs.at(output);
            QMetaObject::invokeMethod(p->engine, "setAudioOutput", Q_ARG(const OutputCreator&, outputCreator));
        }
    });
    QObject::connect(this, &EngineHandler::deviceChanged, p->engine, &AudioEngine::setOutputDevice);

    p->settings->subscribe<Settings::Core::AudioOutput>(this,
                                                        [this](const QString& output) { p->changeOutput(output); });
    p->settings->subscribe<Settings::Core::OutputVolume>(this, [this](double volume) { p->updateVolume(volume); });
}

EngineHandler::~EngineHandler()
{
    p->engineThread.quit();
    p->engineThread.wait();
}

void EngineHandler::setup()
{
    p->changeOutput(p->settings->value<Settings::Core::AudioOutput>());
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

    if(auto out = p->outputs.at(output)()) {
        return out->getAllDevices();
    }

    return {};
}

void EngineHandler::addOutput(const AudioOutputBuilder& output)
{
    if(p->outputs.contains(output.name)) {
        qDebug() << QString{QStringLiteral("Output (%1) already registered")}.arg(output.name);
        return;
    }
    p->outputs.emplace(output.name, output.creator);
}

std::unique_ptr<AudioDecoder> EngineHandler::createDecoder()
{
    return std::make_unique<FFmpegDecoder>();
}
} // namespace Fooyin

#include "moc_enginehandler.cpp"
