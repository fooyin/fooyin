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

#include "nowplayingoutputservice.h"

#include "internalguisettings.h"

#include <core/player/playercontroller.h>
#include <core/scripting/scriptenvironmenthelpers.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QTextStream>

Q_LOGGING_CATEGORY(NOWPLAYINGOUTPUT, "fy.nowplayingoutput")

using namespace Qt::StringLiterals;

namespace {
void trimOutputFile(const QString& path, int lineLimit)
{
    if(lineLimit <= 0) {
        return;
    }

    QFile file{path};
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(NOWPLAYINGOUTPUT) << "Failed to read output file for trimming:" << path << file.errorString();
        return;
    }

    QTextStream input{&file};
    const QString text = input.readAll();
    if(input.status() != QTextStream::Ok) {
        qCWarning(NOWPLAYINGOUTPUT) << "Failed to read output file for trimming:" << path;
        return;
    }

    QStringList lines = text.split(u'\n');
    if(text.endsWith(u'\n') && !lines.empty()) {
        lines.removeLast();
    }
    if(lines.size() <= lineLimit) {
        return;
    }

    lines = lines.sliced(lines.size() - lineLimit);

    QSaveFile trimmedFile{path};
    if(!trimmedFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(NOWPLAYINGOUTPUT) << "Failed to trim output file:" << path << trimmedFile.errorString();
        return;
    }

    QTextStream output{&trimmedFile};
    output << lines.join(u'\n') << u'\n';
    if(output.status() != QTextStream::Ok || !trimmedFile.commit()) {
        qCWarning(NOWPLAYINGOUTPUT) << "Failed to trim output file:" << path << trimmedFile.errorString();
    }
}
} // namespace

namespace Fooyin {
NowPlayingOutputService::NowPlayingOutputService(PlayerController* playerController, SettingsManager* settings,
                                                 QObject* parent)
    : QObject{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_enabled{m_settings->value<Settings::Gui::Internal::NowPlayingOutputEnabled>()}
    , m_events{m_settings->value<Settings::Gui::Internal::NowPlayingOutputUpdateEvents>()}
    , m_targets{m_settings->value<Settings::Gui::Internal::NowPlayingOutputTargets>()}
    , m_options{m_settings->value<Settings::Gui::Internal::NowPlayingOutputOptions>()}
    , m_filepath{m_settings->value<Settings::Gui::Internal::NowPlayingOutputFilePath>()}
    , m_script{m_settings->value<Settings::Gui::Internal::NowPlayingOutputScript>()}
{
    const auto updateIf = [this](UpdateEvent event) {
        if(m_events.testFlag(event)) {
            update();
        }
    };

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                     [updateIf]() { updateIf(UpdateTrack); });
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this,
                     [updateIf]() { updateIf(UpdateTrack); });
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     [updateIf](Player::PlayState state) {
                         if(state == Player::PlayState::Stopped) {
                             updateIf(UpdateStop);
                             return;
                         }
                         updateIf(UpdatePlayPause);
                     });
    QObject::connect(m_playerController, &PlayerController::positionChangedSeconds, this,
                     [updateIf]() { updateIf(UpdateSecond); });

    m_settings->subscribe<Settings::Gui::Internal::NowPlayingOutputEnabled>(this, [this](bool enabled) {
        if(std::exchange(m_enabled, enabled) != enabled) {
            update();
        }
    });
    m_settings->subscribe<Settings::Gui::Internal::NowPlayingOutputUpdateEvents>(
        this, [this](int events) { m_events = UpdateEvents::fromInt(events); });
    m_settings->subscribe<Settings::Gui::Internal::NowPlayingOutputTargets>(
        this, [this](int events) { m_targets = OutputTargets::fromInt(events); });
    m_settings->subscribe<Settings::Gui::Internal::NowPlayingOutputOptions>(
        this, [this](int events) { m_options = OutputOptions::fromInt(events); });
    m_settings->subscribe<Settings::Gui::Internal::NowPlayingOutputScript>(this, [this](const QString& script) {
        if(std::exchange(m_script, script) != script) {
            update();
        }
    });
    m_settings->subscribe<Settings::Gui::Internal::NowPlayingOutputFilePath>(this, [this](const QString& path) {
        if(std::exchange(m_filepath, path) != path) {
            update();
        }
    });
}

NowPlayingOutputService::~NowPlayingOutputService() = default;

void NowPlayingOutputService::update()
{
    if(!m_enabled) {
        return;
    }

    writeOutput(formattedOutput());
}

QString NowPlayingOutputService::formattedOutput()
{
    const Track track = m_playerController ? m_playerController->currentTrack() : Track{};
    if(!track.isValid()) {
        return {};
    }

    auto playbackContext = makePlaybackScriptContext(m_playerController, nullptr, TrackListContextPolicy::Placeholder);
    playbackContext.context.track = &track;
    return m_scriptParser.evaluate(m_script, track, playbackContext.context);
}

void NowPlayingOutputService::writeOutput(const QString& output)
{
    const bool appendToFile = m_options.testFlag(OutputAppendFile);
    if(!appendToFile && output == m_lastOutput) {
        return;
    }

    if(m_targets.testFlag(OutputClipboard)) {
        writeClipboard(output);
    }
    if(m_targets.testFlag(OutputFile)) {
        writeFile(output);
    }

    m_lastOutput = output;
}

void NowPlayingOutputService::writeClipboard(const QString& output) const
{
    if(auto* clipboard = QApplication::clipboard()) {
        clipboard->setText(output);
    }
}

void NowPlayingOutputService::writeFile(const QString& output) const
{
    if(m_filepath.isEmpty()) {
        return;
    }

    const QFileInfo fileInfo{m_filepath};
    const QDir dir = fileInfo.dir();
    if(!dir.exists() && !dir.mkpath(u"."_s)) {
        qCWarning(NOWPLAYINGOUTPUT) << "Failed to create output directory:" << dir.absolutePath();
        return;
    }

    if(m_options.testFlag(OutputAppendFile)) {
        if(output.isEmpty()) {
            return;
        }

        QFile file{m_filepath};
        if(!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qCWarning(NOWPLAYINGOUTPUT) << "Failed to open output file for append:" << m_filepath << file.errorString();
            return;
        }

        QTextStream stream{&file};
        stream << output << u'\n';
        if(stream.status() != QTextStream::Ok) {
            qCWarning(NOWPLAYINGOUTPUT) << "Failed to append output file:" << m_filepath;
            return;
        }

        file.close();
        trimOutputFile(m_filepath, m_settings->value<Settings::Gui::Internal::NowPlayingOutputAppendLineLimit>());
        return;
    }

    QSaveFile file{m_filepath};
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(NOWPLAYINGOUTPUT) << "Failed to open output file:" << m_filepath << file.errorString();
        return;
    }

    QTextStream stream{&file};
    stream << output;
    if(stream.status() != QTextStream::Ok || !file.commit()) {
        qCWarning(NOWPLAYINGOUTPUT) << "Failed to write output file:" << m_filepath << file.errorString();
    }
}
} // namespace Fooyin

#include "moc_nowplayingoutputservice.cpp"
