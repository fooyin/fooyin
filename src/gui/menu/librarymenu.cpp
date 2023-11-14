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

#include <core/library/musiclibrary.h>
#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>

namespace Fy::Gui {
LibraryMenu::LibraryMenu(Utils::ActionManager* actionManager, Core::Library::MusicLibrary* library,
                         Utils::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_library{library}
    , m_settings{settings}
{
    auto* libraryMenu = m_actionManager->actionContainer(Gui::Constants::Menus::Library);

    auto* rescanLibrary
        = new QAction(QIcon::fromTheme(Gui::Constants::Icons::RescanLibrary), tr("&Rescan Library"), this);
    libraryMenu->addAction(m_actionManager->registerAction(rescanLibrary, Gui::Constants::Actions::Rescan));
    QObject::connect(rescanLibrary, &QAction::triggered, m_library, &Core::Library::MusicLibrary::reloadAll);

    auto* openSettings = new QAction(QIcon::fromTheme(Gui::Constants::Icons::Settings), tr("&Settings"), this);
    libraryMenu->addAction(actionManager->registerAction(openSettings, "Library.Settings"));
    QObject::connect(openSettings, &QAction::triggered, this,
                     [this]() { m_settings->settingsDialog()->openAtPage(Gui::Constants::Page::LibraryGeneral); });
}
} // namespace Fy::Gui

#include "moc_librarymenu.cpp"
