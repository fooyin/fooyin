/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "filemenu.h"

#include "core/application.h"
#include "gui/guiconstants.h"

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QApplication>

namespace Fooyin {

FileMenu::FileMenu(ActionManager* actionManager, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
{
    auto* fileMenu = m_actionManager->actionContainer(Constants::Menus::File);

    auto* addFiles = new QAction(tr("Add &files…"), this);
    addFiles->setStatusTip(tr("Add the specified files to the current playlist"));
    auto* addFilesCommand = m_actionManager->registerAction(addFiles, Constants::Actions::AddFiles);
    fileMenu->addAction(addFilesCommand, Actions::Groups::One);
    QObject::connect(addFiles, &QAction::triggered, this, &FileMenu::requestAddFiles);

    auto* addFolders = new QAction(tr("Ad&d folders…"), this);
    addFolders->setStatusTip(tr("Add the contents of the specified directory to the current playlist"));
    auto* addFoldersCommand = m_actionManager->registerAction(addFolders, Constants::Actions::AddFolders);
    fileMenu->addAction(addFoldersCommand, Actions::Groups::One);
    QObject::connect(addFolders, &QAction::triggered, this, &FileMenu::requestAddFolders);

    fileMenu->addSeparator();

    auto* newPlaylist = new QAction(tr("&New playlist"), this);
    newPlaylist->setStatusTip(tr("Create a new empty playlist"));
    auto* newPlaylistCommand = m_actionManager->registerAction(newPlaylist, Constants::Actions::NewPlaylist);
    newPlaylistCommand->setDefaultShortcut(QKeySequence::New);
    fileMenu->addAction(newPlaylistCommand, Actions::Groups::Two);
    QObject::connect(newPlaylist, &QAction::triggered, this, &FileMenu::requestNewPlaylist);

    auto* loadPlaylist = new QAction(tr("&Load playlist…"), this);
    loadPlaylist->setStatusTip(tr("Load the playlist from the specified file"));
    auto* loadPlaylistCommand = m_actionManager->registerAction(loadPlaylist, Constants::Actions::LoadPlaylist);
    fileMenu->addAction(loadPlaylistCommand, Actions::Groups::Two);
    QObject::connect(loadPlaylist, &QAction::triggered, this, &FileMenu::requestLoadPlaylist);

    auto* savePlaylist = new QAction(tr("&Save playlist…"), this);
    savePlaylist->setStatusTip(tr("Save the current playlist to the specified file"));
    auto* savePlaylistCommand = m_actionManager->registerAction(savePlaylist, Constants::Actions::SavePlaylist);
    fileMenu->addAction(savePlaylistCommand, Actions::Groups::Two);
    QObject::connect(savePlaylist, &QAction::triggered, this, &FileMenu::requestSavePlaylist);

    auto* saveAllPlaylists = new QAction(tr("Save &all playlists…"), this);
    saveAllPlaylists->setStatusTip(tr("Save all playlists to the specified location"));
    auto* saveAllPlaylistsCmd = m_actionManager->registerAction(saveAllPlaylists, Constants::Actions::SaveAllPlaylists);
    fileMenu->addAction(saveAllPlaylistsCmd, Actions::Groups::Two);
    QObject::connect(saveAllPlaylists, &QAction::triggered, this, &FileMenu::requestSaveAllPlaylists);

    fileMenu->addSeparator();

    auto* quit = new QAction(Utils::iconFromTheme(Constants::Icons::Quit), tr("&Quit"), this);
    quit->setStatusTip(tr("Quit %1").arg(QStringLiteral("fooyin")));
    auto* quitCommand = m_actionManager->registerAction(quit, Constants::Actions::Exit);
    quitCommand->setDefaultShortcut(QKeySequence::Quit);
    fileMenu->addAction(quitCommand, Actions::Groups::Three);
    QObject::connect(quit, &QAction::triggered, this, &FileMenu::requestExit);
}

} // namespace Fooyin

#include "moc_filemenu.cpp"
