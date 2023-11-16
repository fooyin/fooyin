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

#include "viewmenu.h"

#include "sandbox/sandboxdialog.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QIcon>

namespace Fooyin {
ViewMenu::ViewMenu(ActionManager* actionManager, TrackSelectionController* trackSelection, SettingsManager* settings,
                   QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_trackSelection{trackSelection}
    , m_settings{settings}
{
    auto* viewMenu = m_actionManager->actionContainer(Constants::Menus::View);

    const QIcon layoutEditingIcon = QIcon::fromTheme(Constants::Icons::LayoutEditing);
    m_layoutEditing               = new QAction(layoutEditingIcon, tr("Layout &Editing Mode"), this);
    viewMenu->addAction(m_actionManager->registerAction(m_layoutEditing, Constants::Actions::LayoutEditing));
    QObject::connect(m_layoutEditing, &QAction::triggered, this,
                     [this](bool checked) { m_settings->set<Gui::Settings::LayoutEditing>(checked); });
    m_settings->subscribe<Gui::Settings::LayoutEditing>(m_layoutEditing, &QAction::setChecked);
    m_layoutEditing->setCheckable(true);
    m_layoutEditing->setChecked(m_settings->value<Gui::Settings::LayoutEditing>());

    const QIcon quickSetupIcon = QIcon::fromTheme(Constants::Icons::QuickSetup);
    m_openQuickSetup           = new QAction(quickSetupIcon, tr("&Quick Setup"), this);
    viewMenu->addAction(m_actionManager->registerAction(m_openQuickSetup, Constants::Actions::QuickSetup));
    QObject::connect(m_openQuickSetup, &QAction::triggered, this, &ViewMenu::openQuickSetup);

    m_showSandbox = new QAction(tr("&Script Sandbox"), this);
    viewMenu->addAction(m_actionManager->registerAction(m_showSandbox, Constants::Actions::ScriptSandbox),
                        Actions::Groups::Three);
    QObject::connect(m_showSandbox, &QAction::triggered, this, [this]() {
        auto* sandboxDialog = new SandboxDialog(m_trackSelection, m_settings);
        sandboxDialog->setAttribute(Qt::WA_DeleteOnClose);
        sandboxDialog->show();
    });
}
} // namespace Fooyin

#include "moc_viewmenu.cpp"
