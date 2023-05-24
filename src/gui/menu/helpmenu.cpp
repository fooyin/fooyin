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

#include "helpmenu.h"

#include "aboutdialog.h"
#include "gui/guiconstants.h"

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>

#include <QAction>
#include <QIcon>

namespace Fy::Gui {
HelpMenu::HelpMenu(Utils::ActionManager* actionManager, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
{
    auto* helpMenu = m_actionManager->actionContainer(Constants::Menus::Help);

    const auto aboutIcon = QIcon::fromTheme(Constants::Icons::Fooyin);
    m_about              = new QAction(aboutIcon, tr("&About"), this);
    m_actionManager->registerAction(m_about, Constants::Actions::About);
    helpMenu->addAction(m_about, Constants::Groups::Three);
    connect(m_about, &QAction::triggered, this, &HelpMenu::showAboutDialog);
}

void HelpMenu::showAboutDialog()
{
    auto* aboutDialog = new AboutDialog();
    connect(aboutDialog, &QDialog::finished, aboutDialog, &QObject::deleteLater);
    aboutDialog->show();
}
} // namespace Fy::Gui
