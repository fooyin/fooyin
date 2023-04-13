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

#include "librarymenu.h"

#include "gui/guiconstants.h"

#include <core/library/musiclibrary.h>

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QMenu>

namespace Fy::Gui {
LibraryMenu::LibraryMenu(Utils::ActionManager* actionManager, Core::Library::MusicLibrary* library,
                         Utils::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_library{library}
    , m_settings{settings}
{
    auto* libraryMenu = m_actionManager->actionContainer(Gui::Constants::Menus::Library);

    const QIcon rescanIcon   = QIcon(Gui::Constants::Icons::RescanLibrary);
    const QIcon settingsIcon = QIcon(Gui::Constants::Icons::Settings);

    m_rescanLibrary = new QAction(rescanIcon, tr("&Rescan Library"), this);
    m_actionManager->registerAction(m_rescanLibrary, Gui::Constants::Actions::Rescan);
    libraryMenu->addAction(m_rescanLibrary, Gui::Constants::Groups::Two);
    connect(m_rescanLibrary, &QAction::triggered, m_library, &Core::Library::MusicLibrary::reloadAll);

    m_openSettings = new QAction(settingsIcon, tr("&Settings"), this);
    actionManager->registerAction(m_openSettings, Gui::Constants::Actions::Settings);
    libraryMenu->addAction(m_openSettings, Gui::Constants::Groups::Three);
    connect(m_openSettings, &QAction::triggered, this, [this]() {
        m_settings->settingsDialog()->openAtPage(Gui::Constants::Page::LibraryGeneral);
    });
}
} // namespace Fy::Gui
