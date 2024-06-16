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

#include "audioplaybackengine.h"
#include "engine/ffmpeg/ffmpegdecoder.h"

#include <core/coresettings.h>
#include <core/engine/audioengine.h>
#include <core/engine/outputplugin.h>
#include <core/track.h>

#include <core/player/playercontroller.h>
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
    EngineHandler* m_self;

    PlayerController* m_playerController;
    SettingsManager* m_settings;

    QThread m_engineThread;
    AudioEngine* m_engine;

    std::map<QString, OutputCreator> m_outputs;
    CurrentOutput m_currentOutput;

    Private(EngineHandler* self, PlayerController* playerController, SettingsManager* settings)
        : m_self{self}
        , m_playerController{playerController}
        , m_settings{settings}
        , m_engine{new AudioPlaybackEngine(m_settings)}
    {
        m_engine->moveToThread(&m_engineThread);
        m_engineThread.start();

        QObject::connect(m_playerController, &PlayerController::currentTrackChanged, m_engine,
                         &AudioEngine::changeTrack);
        QObject::connect(m_playerController, &PlayerController::positionMoved, m_engine, &AudioEngine::seek);
        QObject::connect(&m_engineThread, &QThread::finished, m_engine, &AudioEngine::deleteLater);
        QObject::connect(m_engine, &AudioEngine::trackAboutToFinish, m_self, &EngineHandler::trackAboutToFinish);
        QObject::connect(m_engine, &AudioEngine::positionChanged, m_playerController,
                         &PlayerController::setCurrentPosition);
        QObject::connect(m_engine, &AudioEngine::stateChanged, m_self,
                         [this](PlaybackState state) { handleStateChange(state); });
        QObject::connect(m_engine, &AudioEngine::trackStatusChanged, m_self,
                         [this](TrackStatus status) { handleTrackStatus(status); });

        updateVolume(m_settings->value<Settings::Core::OutputVolume>());
    }

    void handleStateChange(PlaybackState state) const
    {
        switch(state) {
            case(PlaybackState::Error):
            case(PlaybackState::Stopped):
                m_playerController->stop();
                break;
            case(PlaybackState::Paused):
                m_playerController->pause();
                break;
            case(PlaybackState::Playing):
                break;
        }
    }

    void handleTrackStatus(TrackStatus status) const
    {
        switch(status) {
            case(TrackStatus::EndOfTrack):
                m_playerController->next();
                break;
            case(TrackStatus::NoTrack):
                m_playerController->stop();
                break;
            case(TrackStatus::InvalidTrack):
            case(TrackStatus::LoadingTrack):
            case(TrackStatus::LoadedTrack):
            case(TrackStatus::BufferedTrack):
                break;
        }

        emit m_self->trackStatusChanged(status);
    }

    void playStateChanged(PlayState state)
    {
        QMetaObject::invokeMethod(
            m_engine,
            [this, state]() {
                switch(state) {
                    case(PlayState::Playing):
                        m_engine->play();
                        break;
                    case(PlayState::Paused):
                        m_engine->pause();
                        break;
                    case(PlayState::Stopped):
                        m_engine->stop();
                        break;
                }
            },
            Qt::QueuedConnection);
    }

    void changeOutput(const QString& output)
    {
        auto loadDefault = [this]() {
            m_currentOutput = {m_outputs.cbegin()->first, QStringLiteral("default")};
            emit m_self->outputChanged(m_currentOutput.name, m_currentOutput.device);
        };

        if(output.isEmpty()) {
            if(m_outputs.empty() || !m_currentOutput.name.isEmpty()) {
                return;
            }
            loadDefault();
        }

        const QStringList newOutput = output.split(QStringLiteral("|"));

        if(newOutput.empty() || newOutput.size() < 2) {
            return;
        }

        const QString& newName = newOutput.at(0);
        const QString& device  = newOutput.at(1);

        if(m_outputs.empty()) {
            qWarning() << "No Outputs have been registered";
            return;
        }

        if(!m_outputs.contains(newName)) {
            qWarning() << QStringLiteral("Output (%1) hasn't been registered").arg(newName);
            loadDefault();
            return;
        }

        if(m_currentOutput.name != newName) {
            m_currentOutput = {newName, device};
            emit m_self->outputChanged(newName, device);
        }
        else if(m_currentOutput.device != device) {
            m_currentOutput.device = device;
            emit m_self->deviceChanged(device);
        }
    }

    void updateVolume(double volume)
    {
        QMetaObject::invokeMethod(
            m_engine, [this, volume]() { m_engine->setVolume(volume); }, Qt::QueuedConnection);
    }
};

EngineHandler::EngineHandler(PlayerController* playerController, SettingsManager* settings, QObject* parent)
    : EngineController{parent}
    , p{std::make_unique<Private>(this, playerController, settings)}
{
    QObject::connect(playerController, &PlayerController::playStateChanged, this,
                     [this](PlayState state) { p->playStateChanged(state); });

    QObject::connect(this, &EngineHandler::outputChanged, this, [this](const QString& output, const QString& device) {
        if(p->m_outputs.contains(output)) {
            const auto& outputCreator = p->m_outputs.at(output);
            QMetaObject::invokeMethod(
                p->m_engine, [this, outputCreator, device]() { p->m_engine->setAudioOutput(outputCreator, device); });
        }
    });
    QObject::connect(this, &EngineHandler::deviceChanged, p->m_engine, &AudioEngine::setOutputDevice);

    p->m_settings->subscribe<Settings::Core::AudioOutput>(this,
                                                          [this](const QString& output) { p->changeOutput(output); });
    p->m_settings->subscribe<Settings::Core::OutputVolume>(this, [this](double volume) { p->updateVolume(volume); });
}

EngineHandler::~EngineHandler()
{
    p->m_engineThread.quit();
    p->m_engineThread.wait();
}

void EngineHandler::setup()
{
    p->changeOutput(p->m_settings->value<Settings::Core::AudioOutput>());
}

OutputNames EngineHandler::getAllOutputs() const
{
    OutputNames outputs;

    for(const auto& [name, output] : p->m_outputs) {
        outputs.emplace_back(name);
    }

    return outputs;
}

OutputDevices EngineHandler::getOutputDevices(const QString& output) const
{
    if(!p->m_outputs.contains(output)) {
        qDebug() << "Output not found: " << output;
        return {};
    }

    if(auto out = p->m_outputs.at(output)()) {
        return out->getAllDevices();
    }

    return {};
}

void EngineHandler::addOutput(const AudioOutputBuilder& output)
{
    if(p->m_outputs.contains(output.name)) {
        qDebug() << QStringLiteral("Output (%1) already registered").arg(output.name);
        return;
    }
    p->m_outputs.emplace(output.name, output.creator);
}

std::unique_ptr<AudioDecoder> EngineHandler::createDecoder()
{
    return std::make_unique<FFmpegDecoder>();
}
} // namespace Fooyin

#include "moc_enginehandler.cpp"
