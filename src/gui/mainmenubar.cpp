/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "mainmenubar.h"

#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>

#include <QMenu>

namespace Fooyin {
MainMenuBar::MainMenuBar(ActionManager* actionManager, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_menubar{m_actionManager->createMenuBar(Constants::MenuBar)}
{
    m_menubar->appendGroup(Constants::Groups::File);
    m_menubar->appendGroup(Constants::Groups::Edit);
    m_menubar->appendGroup(Constants::Groups::View);
    m_menubar->appendGroup(Constants::Groups::Playback);
    m_menubar->appendGroup(Constants::Groups::Library);
    m_menubar->appendGroup(Constants::Groups::Help);

    ActionContainer* fileMenu = m_actionManager->createMenu(Constants::Menus::File);
    m_menubar->addMenu(fileMenu, Constants::Groups::File);
    fileMenu->menu()->setTitle(ActionManager::tr("&File"));

    ActionContainer* editMenu = m_actionManager->createMenu(Constants::Menus::Edit);
    m_menubar->addMenu(editMenu, Constants::Groups::Edit);
    editMenu->menu()->setTitle(ActionManager::tr("&Edit"));

    ActionContainer* viewMenu = m_actionManager->createMenu(Constants::Menus::View);
    m_menubar->addMenu(viewMenu, Constants::Groups::View);
    viewMenu->menu()->setTitle(ActionManager::tr("&View"));

    ActionContainer* playbackMenu = m_actionManager->createMenu(Constants::Menus::Playback);
    m_menubar->addMenu(playbackMenu, Constants::Groups::Playback);
    playbackMenu->menu()->setTitle(ActionManager::tr("&Playback"));

    ActionContainer* libraryMenu = m_actionManager->createMenu(Constants::Menus::Library);
    m_menubar->addMenu(libraryMenu, Constants::Groups::Library);
    libraryMenu->menu()->setTitle(ActionManager::tr("&Library"));

    ActionContainer* helpMenu = m_actionManager->createMenu(Constants::Menus::Help);
    m_menubar->addMenu(helpMenu, Constants::Groups::Help);
    helpMenu->menu()->setTitle(ActionManager::tr("&Help"));
}

QMenuBar* MainMenuBar::menuBar() const
{
    return m_menubar->menuBar();
}
} // namespace Fooyin

#include "moc_mainmenubar.cpp"
