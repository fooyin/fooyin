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

#include "sandbox/sandboxdialog.h"

#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>

#include <QAction>

namespace Fy::Gui {
EditMenu::EditMenu(Utils::ActionManager* actionManager, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
{
    auto* editMenu = m_actionManager->actionContainer(Gui::Constants::Menus::Edit);

    m_showSandbox = new QAction(tr("Open &Script Sandbox"), this);
    m_actionManager->registerAction(m_showSandbox, Gui::Constants::Actions::Rescan);
    editMenu->addAction(m_showSandbox, Gui::Constants::Groups::Two);
    connect(m_showSandbox, &QAction::triggered, this, []() {
        auto* sandboxDialog = new Sandbox::SandboxDialog();
        sandboxDialog->setAttribute(Qt::WA_DeleteOnClose);
        sandboxDialog->show();
    });
}
} // namespace Fy::Gui
