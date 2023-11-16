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

#include "editmenu.h"

#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>

namespace Fooyin {
EditMenu::EditMenu(ActionManager* actionManager, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
{
    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    auto* openSettings    = new QAction(QIcon::fromTheme(Constants::Icons::Settings), tr("&Settings"), this);
    auto* settingsCommand = actionManager->registerAction(openSettings, Constants::Actions::Settings);
    settingsCommand->setDefaultShortcut(QKeySequence{Qt::CTRL | Qt::Key_P});
    editMenu->addSeparator(Actions::Groups::Three);
    editMenu->addAction(settingsCommand, Actions::Groups::Three);
    QObject::connect(openSettings, &QAction::triggered, m_settings->settingsDialog(), &SettingsDialogController::open);
}
} // namespace Fooyin
