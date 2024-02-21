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

#include "helpmenu.h"

#include "aboutdialog.h"

#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QIcon>

namespace {
void showAboutDialog()
{
    auto* aboutDialog = new Fooyin::AboutDialog();
    QObject::connect(aboutDialog, &QDialog::finished, aboutDialog, &QObject::deleteLater);
    aboutDialog->show();
}
} // namespace

namespace Fooyin {
HelpMenu::HelpMenu(ActionManager* actionManager, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
{
    auto* helpMenu = m_actionManager->actionContainer(Constants::Menus::Help);

    m_about = new QAction(Utils::iconFromTheme(Constants::Icons::Fooyin), tr("&About"), this);
    helpMenu->addAction(m_actionManager->registerAction(m_about, Constants::Actions::About), Actions::Groups::Three);
    QObject::connect(m_about, &QAction::triggered, this, showAboutDialog);
}
} // namespace Fooyin

#include "moc_helpmenu.cpp"
