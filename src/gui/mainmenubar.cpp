/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include <core/actions/actioncontainer.h>
#include <core/actions/actionmanager.h>
#include <core/constants.h>

#include <QMenu>

namespace Gui {
MainMenuBar::MainMenuBar(Core::ActionManager* actionManager, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_menubar{m_actionManager->createMenuBar(Core::Constants::MenuBar)}
{
    m_menubar->appendGroup(Core::Constants::Groups::File);
    m_menubar->appendGroup(Core::Constants::Groups::Edit);
    m_menubar->appendGroup(Core::Constants::Groups::View);
    m_menubar->appendGroup(Core::Constants::Groups::Playback);
    m_menubar->appendGroup(Core::Constants::Groups::Library);
    m_menubar->appendGroup(Core::Constants::Groups::Help);

    Core::ActionContainer* fileMenu = m_actionManager->createMenu(Core::Constants::Menus::File);
    m_menubar->addMenu(fileMenu, Core::Constants::Groups::File);
    fileMenu->menu()->setTitle(tr("&File"));
    fileMenu->appendGroup(Core::Constants::Groups::Three);

    Core::ActionContainer* editMenu = m_actionManager->createMenu(Core::Constants::Menus::Edit);
    m_menubar->addMenu(editMenu, Core::Constants::Groups::Edit);
    editMenu->menu()->setTitle(tr("&Edit"));

    Core::ActionContainer* viewMenu = m_actionManager->createMenu(Core::Constants::Menus::View);
    m_menubar->addMenu(viewMenu, Core::Constants::Groups::View);
    viewMenu->menu()->setTitle(tr("&View"));
    viewMenu->appendGroup(Core::Constants::Groups::Three);

    Core::ActionContainer* playbackMenu = m_actionManager->createMenu(Core::Constants::Menus::Playback);
    m_menubar->addMenu(playbackMenu, Core::Constants::Groups::Playback);
    playbackMenu->menu()->setTitle(tr("&Playback"));

    Core::ActionContainer* libraryMenu = m_actionManager->createMenu(Core::Constants::Menus::Library);
    m_menubar->addMenu(libraryMenu, Core::Constants::Groups::Library);
    libraryMenu->menu()->setTitle(tr("&Library"));
    libraryMenu->appendGroup(Core::Constants::Groups::Two);
    libraryMenu->appendGroup(Core::Constants::Groups::Three);

    Core::ActionContainer* helpMenu = m_actionManager->createMenu(Core::Constants::Menus::Help);
    m_menubar->addMenu(helpMenu, Core::Constants::Groups::Help);
    helpMenu->menu()->setTitle(tr("&Help"));
}

QMenuBar* MainMenuBar::menuBar() const
{
    return m_menubar->menuBar();
}
} // namespace Gui
