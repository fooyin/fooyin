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

#include "editmenu.h"

#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>

namespace Fooyin {
EditMenu::EditMenu(ActionManager* actionManager, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
{
    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    auto* search = new QAction(tr("Sear&ch"), this);
    search->setStatusTip(tr("Search the current playlist"));
    auto* searchCommand = actionManager->registerAction(search, Constants::Actions::Search);
    searchCommand->setDefaultShortcut({{QKeySequence::Find}, {QKeySequence{Qt::Key_F3}}});
    editMenu->addAction(searchCommand, Actions::Groups::Three);
    QObject::connect(search, &QAction::triggered, this, &EditMenu::requestSearch);

    auto* openSettings = new QAction(Utils::iconFromTheme(Constants::Icons::Settings), tr("&Settings"), this);
    openSettings->setStatusTip(tr("Open the settings dialog"));
    auto* settingsCommand = actionManager->registerAction(openSettings, Constants::Actions::Settings);
    settingsCommand->setDefaultShortcut(QKeySequence{Qt::CTRL | Qt::Key_P});
    editMenu->addSeparator(Actions::Groups::Three);
    editMenu->addAction(settingsCommand, Actions::Groups::Three);
    QObject::connect(openSettings, &QAction::triggered, m_settings->settingsDialog(), &SettingsDialogController::open);
}
} // namespace Fooyin

#include "moc_editmenu.cpp"
