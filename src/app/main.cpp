/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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

#include "version.h"

#include "commandline.h"

#include <core/application.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiapplication.h>
#include <utils/logging/messagehandler.h>

#include <kdsingleapplication.h>

#include <QApplication>
#include <QDir>
#include <QLoggingCategory>

#include <QSurfaceFormat>

#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace Qt::StringLiterals;

namespace {
void configureOpenGLSurfaceFormat()
{
    QSurfaceFormat format{QSurfaceFormat::defaultFormat()};
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setAlphaBufferSize(8);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);
}

#ifdef Q_OS_WIN
#include <roapi.h>
#include <windows.h>

struct GuiThreadApartment
{
    HRESULT result{E_UNEXPECTED};

    GuiThreadApartment()
    {
        result = RoInitialize(RO_INIT_SINGLETHREADED);
    }

    ~GuiThreadApartment()
    {
        if(SUCCEEDED(result)) {
            RoUninitialize();
        }
    }
};

void configurePluginSearchPaths()
{
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

    const QDir appPath{QCoreApplication::applicationDirPath()};
    const QString pluginDir = appPath.absolutePath() + u"/plugins"_s;
    AddDllDirectory(reinterpret_cast<LPCWSTR>(pluginDir.utf16()));
}
#endif

void parseCmdOptions(Fooyin::Application& app, Fooyin::GuiApplication& guiApp, CommandLine& cmdLine)
{
    const auto playerAction = cmdLine.playerAction();
    if(playerAction != CommandLine::PlayerAction::None) {
        auto* player = app.playerController();
        switch(playerAction) {
            case CommandLine::PlayerAction::PlayPause:
                player->playPause();
                break;
            case CommandLine::PlayerAction::Play:
                player->play();
                break;
            case CommandLine::PlayerAction::Pause:
                player->pause();
                break;
            case CommandLine::PlayerAction::Stop:
                player->stop();
                break;
            case CommandLine::PlayerAction::Next:
                player->next();
                break;
            case CommandLine::PlayerAction::Previous:
                player->previous();
                break;
            case CommandLine::PlayerAction::SeekFwd:
                player->seekForward(cmdLine.seekDelta());
                break;
            case CommandLine::PlayerAction::SeekBack:
                player->seekBackward(cmdLine.seekDelta());
                break;
            case CommandLine::PlayerAction::None:
                break;
        }
    }

    switch(cmdLine.volumeAction()) {
        case CommandLine::VolumeAction::Set:
            guiApp.setVolume(cmdLine.volume());
            break;
        case CommandLine::VolumeAction::Increase:
            guiApp.increaseVolume();
            break;
        case CommandLine::VolumeAction::Decrease:
            guiApp.decreaseVolume();
            break;
        case CommandLine::VolumeAction::ToggleMute:
            guiApp.toggleMute();
            break;
        case CommandLine::VolumeAction::None:
            break;
    }

    auto playMode = app.playerController()->playMode();
    bool playModeChanged{false};

    if(cmdLine.repeatMode() != CommandLine::RepeatMode::Unchanged) {
        playMode &= ~(Fooyin::Playlist::RepeatTrack | Fooyin::Playlist::RepeatAlbum | Fooyin::Playlist::RepeatPlaylist);
        switch(cmdLine.repeatMode()) {
            case CommandLine::RepeatMode::Playlist:
                playMode |= Fooyin::Playlist::RepeatPlaylist;
                break;
            case CommandLine::RepeatMode::Album:
                playMode |= Fooyin::Playlist::RepeatAlbum;
                break;
            case CommandLine::RepeatMode::Track:
                playMode |= Fooyin::Playlist::RepeatTrack;
                break;
            case CommandLine::RepeatMode::Off:
            case CommandLine::RepeatMode::Unchanged:
                break;
        }
        playModeChanged = true;
    }

    if(cmdLine.shuffleMode() != CommandLine::ShuffleMode::Unchanged) {
        playMode &= ~(Fooyin::Playlist::ShuffleTracks | Fooyin::Playlist::ShuffleAlbums | Fooyin::Playlist::Random);
        switch(cmdLine.shuffleMode()) {
            case CommandLine::ShuffleMode::Tracks:
                playMode |= Fooyin::Playlist::ShuffleTracks;
                break;
            case CommandLine::ShuffleMode::Albums:
                playMode |= Fooyin::Playlist::ShuffleAlbums;
                break;
            case CommandLine::ShuffleMode::Random:
                playMode |= Fooyin::Playlist::Random;
                break;
            case CommandLine::ShuffleMode::Off:
            case CommandLine::ShuffleMode::Unchanged:
                break;
        }
        playModeChanged = true;
    }

    if(playModeChanged) {
        app.playerController()->setPlayMode(playMode);
    }

    const auto files = cmdLine.files();
    if(!files.empty()) {
        guiApp.openFiles(files);
    }
}
} // namespace

int main(int argc, char** argv)
{
#ifdef Q_OS_WIN
    const GuiThreadApartment guiThreadApartment;
    if(FAILED(guiThreadApartment.result)) {
        QLoggingCategory log{"Main"};
        qCCritical(log) << "Failed to initialise the GUI thread apartment:"
                        << u"HRESULT 0x%1"_s.arg(
                               static_cast<qulonglong>(static_cast<uint32_t>(guiThreadApartment.result)), 8, 16,
                               QChar{u'0'});
        return 1;
    }
#endif

    configureOpenGLSurfaceFormat();

    Q_INIT_RESOURCE(data);
    Q_INIT_RESOURCE(icons);

    QCoreApplication::setApplicationName(u"fooyin"_s);
    QCoreApplication::setApplicationVersion(QStringLiteral(VERSION));
    QGuiApplication::setDesktopFileName(u"org.fooyin.fooyin"_s);
    QGuiApplication::setQuitOnLastWindowClosed(false);

#ifdef Q_OS_WIN
    configurePluginSearchPaths();
#endif

    CommandLine commandLine{argc, argv};

    auto checkInstance = [&commandLine](KDSingleApplication& instance) {
        if(instance.isPrimaryInstance()) {
            return true;
        }

        if(commandLine.empty()) {
            QLoggingCategory log{"Main"};
            qCInfo(log) << "fooyin already running";
            instance.sendMessage({});
        }
        else {
            instance.sendMessage(commandLine.saveOptions());
        }

        return commandLine.skipSingleApp();
    };

    {
        const QCoreApplication app{argc, argv};
        KDSingleApplication instance{QCoreApplication::applicationName(),
                                     KDSingleApplication::Option::IncludeUsernameInSocketName};
        const auto parseResult = commandLine.parse();
        if(parseResult != CommandLine::ParseResult::Run) {
            const QByteArray message = commandLine.message().toLocal8Bit();
            if(parseResult == CommandLine::ParseResult::ExitSuccess) {
                std::cout << message.constData() << '\n';
                return 0;
            }

            std::cerr << message.constData() << '\n';
            return 2;
        }
        if(!checkInstance(instance)) {
            return 0;
        }
    }

    const QApplication app{argc, argv};
    Fooyin::MessageHandler::install(nullptr);

    KDSingleApplication instance{QCoreApplication::applicationName(),
                                 KDSingleApplication::Option::IncludeUsernameInSocketName};
    if(!checkInstance(instance)) {
        return 0;
    }

    // Startup
    Fooyin::Application coreApp;
    coreApp.startup();
    Fooyin::GuiApplication guiApp{&coreApp};
    guiApp.startup();

    if(!commandLine.empty()) {
        // Wait until playlists have been populated before parsing in case of playback commands
        auto* playlistHandler = coreApp.playlistHandler();
        QObject::connect(
            playlistHandler, &Fooyin::PlaylistHandler::playlistsPopulated, &guiApp,
            [&]() { parseCmdOptions(coreApp, guiApp, commandLine); }, Qt::SingleShotConnection);
    }

    QObject::connect(&instance, &KDSingleApplication::messageReceived, &guiApp, [&](const QByteArray& options) {
        if(options.isEmpty()) {
            guiApp.raise();
            return;
        }

        CommandLine command;
        if(!command.loadOptions(options)) {
            QLoggingCategory log{"Main"};
            qCWarning(log) << "Received an invalid command line message";
            return;
        }
        if(!command.empty()) {
            parseCmdOptions(coreApp, guiApp, command);
        }
    });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &coreApp, [&coreApp, &guiApp]() {
        guiApp.shutdown();
        coreApp.shutdown();
    });

    return QCoreApplication::exec();
}
