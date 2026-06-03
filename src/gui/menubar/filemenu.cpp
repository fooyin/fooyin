/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>

#include <QAction>
#include <QApplication>

using namespace Qt::StringLiterals;

namespace Fooyin {
FileMenu::FileMenu(ActionManager* actionManager, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
{
    auto* fileMenu = m_actionManager->actionContainer(Constants::Menus::File);

    const QStringList fileCategory = {tr("File")};

    auto* addFiles = new QAction(tr("Add &files…"), this);
    addFiles->setStatusTip(tr("Add the specified files to the current playlist"));
    auto* addFilesCommand = m_actionManager->registerAction(addFiles, Constants::Actions::AddFiles);
    addFilesCommand->setCategories(fileCategory);
    fileMenu->addAction(addFilesCommand, Actions::Groups::One);
    QObject::connect(addFiles, &QAction::triggered, this, &FileMenu::requestAddFiles);

    auto* addFolders = new QAction(tr("Ad&d folders…"), this);
    addFolders->setStatusTip(tr("Add the contents of the specified directory to the current playlist"));
    auto* addFoldersCommand = m_actionManager->registerAction(addFolders, Constants::Actions::AddFolders);
    addFoldersCommand->setCategories(fileCategory);
    fileMenu->addAction(addFoldersCommand, Actions::Groups::One);
    QObject::connect(addFolders, &QAction::triggered, this, &FileMenu::requestAddFolders);

    auto* addStreamUrl = new QAction(tr("Add stream &URL…"), this);
    addStreamUrl->setStatusTip(tr("Add the specified stream URL to the current playlist"));
    auto* addStreamUrlCommand = m_actionManager->registerAction(addStreamUrl, Constants::Actions::AddStreamUrl);
    addStreamUrlCommand->setCategories(fileCategory);
    fileMenu->addAction(addStreamUrlCommand, Actions::Groups::One);
    QObject::connect(addStreamUrl, &QAction::triggered, this, &FileMenu::requestAddStreamUrl);

    fileMenu->addSeparator();

    auto* newPlaylist = new QAction(tr("&New playlist"), this);
    newPlaylist->setStatusTip(tr("Create a new empty playlist"));
    auto* newPlaylistCommand = m_actionManager->registerAction(newPlaylist, Constants::Actions::NewPlaylist);
    newPlaylistCommand->setCategories(fileCategory);
    newPlaylistCommand->setDefaultShortcut(QKeySequence::New);
    fileMenu->addAction(newPlaylistCommand, Actions::Groups::Two);
    QObject::connect(newPlaylist, &QAction::triggered, this, &FileMenu::requestNewPlaylist);

    auto* newAutoPlaylist = new QAction(tr("&New autoplaylist"), this);
    newAutoPlaylist->setStatusTip(tr("Create a new autoplaylist"));
    auto* newAutoPlaylistCommand
        = m_actionManager->registerAction(newAutoPlaylist, Constants::Actions::NewAutoPlaylist);
    newAutoPlaylistCommand->setCategories(fileCategory);
    fileMenu->addAction(newAutoPlaylistCommand, Actions::Groups::Two);
    QObject::connect(newAutoPlaylist, &QAction::triggered, this, &FileMenu::requestNewAutoPlaylist);

    auto* loadPlaylist = new QAction(tr("&Load playlist…"), this);
    loadPlaylist->setStatusTip(tr("Load the playlist from the specified file"));
    auto* loadPlaylistCommand = m_actionManager->registerAction(loadPlaylist, Constants::Actions::LoadPlaylist);
    loadPlaylistCommand->setCategories(fileCategory);
    fileMenu->addAction(loadPlaylistCommand, Actions::Groups::Two);
    QObject::connect(loadPlaylist, &QAction::triggered, this, &FileMenu::requestLoadPlaylist);

    auto* savePlaylist = new QAction(tr("&Save playlist…"), this);
    savePlaylist->setStatusTip(tr("Save the current playlist to the specified file"));
    auto* savePlaylistCommand = m_actionManager->registerAction(savePlaylist, Constants::Actions::SavePlaylist);
    savePlaylistCommand->setCategories(fileCategory);
    fileMenu->addAction(savePlaylistCommand, Actions::Groups::Two);
    QObject::connect(savePlaylist, &QAction::triggered, this, &FileMenu::requestSavePlaylist);

    auto* saveAllPlaylists = new QAction(tr("Save &all playlists…"), this);
    saveAllPlaylists->setStatusTip(tr("Save all playlists to the specified location"));
    auto* saveAllPlaylistsCmd = m_actionManager->registerAction(saveAllPlaylists, Constants::Actions::SaveAllPlaylists);
    saveAllPlaylistsCmd->setCategories(fileCategory);
    fileMenu->addAction(saveAllPlaylistsCmd, Actions::Groups::Two);
    QObject::connect(saveAllPlaylists, &QAction::triggered, this, &FileMenu::requestSaveAllPlaylists);

    fileMenu->addSeparator();

    auto* quit = new QAction(tr("&Quit"), this);
    Gui::setThemeIcon(quit, Constants::Icons::Quit);
    quit->setStatusTip(tr("Quit fooyin"));
    auto* quitCommand = m_actionManager->registerAction(quit, Constants::Actions::Exit);
    quitCommand->setCategories(fileCategory);
    quitCommand->setDefaultShortcut(QKeySequence::Quit);
    fileMenu->addAction(quitCommand, Actions::Groups::Three);
    QObject::connect(quit, &QAction::triggered, this, &FileMenu::requestExit);
}

} // namespace Fooyin

#include "moc_filemenu.cpp"
