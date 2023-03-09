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

#include "filemenu.h"

#include "gui/guiconstants.h"

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>

namespace Fy::Gui {

FileMenu::FileMenu(Utils::ActionManager* actionManager, Utils::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
{
    auto* fileMenu = m_actionManager->actionContainer(Gui::Constants::Menus::File);

    const auto settingsIcon = QIcon(Gui::Constants::Icons::Settings);
    const auto quitIcon     = QIcon(Gui::Constants::Icons::Quit);

    m_openSettings = new QAction(settingsIcon, tr("&Settings"), this);
    actionManager->registerAction(m_openSettings, Gui::Constants::Actions::Settings);
    fileMenu->addAction(m_openSettings, Gui::Constants::Groups::Three);
    connect(m_openSettings, &QAction::triggered, m_settings->settingsDialog(), &Utils::SettingsDialogController::open);

    m_quit = new QAction(quitIcon, tr("E&xit"), this);
    m_actionManager->registerAction(m_quit, Gui::Constants::Actions::Exit);
    fileMenu->addAction(m_quit, Gui::Constants::Groups::Three);
    connect(m_quit, &QAction::triggered, qApp, &QApplication::quit);
}

} // namespace Fy::Gui
