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

#include "filemenu.h"

#include "gui/guiconstants.h"

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QApplication>

namespace Fooyin {

FileMenu::FileMenu(ActionManager* actionManager, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
{
    auto* fileMenu = m_actionManager->actionContainer(Constants::Menus::File);

    auto* quit        = new QAction(QIcon::fromTheme(Constants::Icons::Quit), tr("E&xit"), this);
    auto* quitCommand = m_actionManager->registerAction(quit, Constants::Actions::Exit);
    quitCommand->setDefaultShortcut(QKeySequence::Quit);
    fileMenu->addAction(quitCommand, Actions::Groups::Three);
    QObject::connect(quit, &QAction::triggered, qApp, &QCoreApplication::quit, Qt::QueuedConnection);
}

} // namespace Fooyin

#include "moc_filemenu.cpp"
