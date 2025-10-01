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

#include "version.h"

#include "commandline.h"

#include <core/application.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiapplication.h>

#include <kdsingleapplication.h>

#include <QApplication>
#include <QLoggingCategory>

using namespace Qt::StringLiterals;

namespace {
#ifdef Q_OS_WIN
#include <windows.h>
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
            case(CommandLine::PlayerAction::PlayPause):
                player->playPause();
                break;
            case(CommandLine::PlayerAction::Play):
                player->play();
                break;
            case(CommandLine::PlayerAction::Pause):
                player->pause();
                break;
            case(CommandLine::PlayerAction::Stop):
                player->stop();
                break;
            case(CommandLine::PlayerAction::Next):
                player->next();
                break;
            case(CommandLine::PlayerAction::Previous):
                player->previous();
                break;
            case(CommandLine::PlayerAction::None):
                break;
        }
    }

    const auto files = cmdLine.files();
    if(!files.empty()) {
        guiApp.openFiles(files);
    }
}
} // namespace

int main(int argc, char** argv)
{
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
        if(!commandLine.parse()) {
            return 1;
        }
        if(!checkInstance(instance)) {
            return 0;
        }
    }

    const QApplication app{argc, argv};
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
        const auto delayedParse = [&]() {
            parseCmdOptions(coreApp, guiApp, commandLine);
        };
        auto* playlistHandler = coreApp.playlistHandler();
        QObject::connect(playlistHandler, &Fooyin::PlaylistHandler::playlistsPopulated, playlistHandler,
                         [delayedParse]() { delayedParse(); });
    }

    QObject::connect(&instance, &KDSingleApplication::messageReceived, &guiApp, [&](const QByteArray& options) {
        CommandLine command;
        command.loadOptions(options);
        if(!command.empty()) {
            parseCmdOptions(coreApp, guiApp, command);
        }
        else {
            guiApp.raise();
        }
    });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &coreApp, [&coreApp, &guiApp]() {
        guiApp.shutdown();
        coreApp.shutdown();
    });

    return QCoreApplication::exec();
}
