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

#include "viewmenu.h"

#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>

namespace Fooyin {
ViewMenu::ViewMenu(ActionManager* actionManager, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
{
    auto* viewMenu = m_actionManager->actionContainer(Constants::Menus::View);

    auto* openQuickSetup = new QAction(Utils::iconFromTheme(Constants::Icons::QuickSetup), tr("&Quick setup"), this);
    openQuickSetup->setStatusTip(tr("Open the quick setup dialog"));
    viewMenu->addAction(m_actionManager->registerAction(openQuickSetup, Constants::Actions::QuickSetup),
                        Actions::Groups::One);
    QObject::connect(openQuickSetup, &QAction::triggered, this, &ViewMenu::openQuickSetup);

    viewMenu->addSeparator();

    auto* showLog = new QAction(Utils::iconFromTheme(Constants::Icons::Log), tr("&Log"), this);
    showLog->setStatusTip(tr("Open the log dialog"));
    viewMenu->addAction(m_actionManager->registerAction(showLog, Constants::Actions::Log));
    QObject::connect(showLog, &QAction::triggered, this, &ViewMenu::openLog);

    auto* showEditor = new QAction(Utils::iconFromTheme(Constants::Icons::ScriptEditor), tr("&Script editor"), this);
    showEditor->setStatusTip(tr("Open the script editor dialog"));
    viewMenu->addAction(m_actionManager->registerAction(showEditor, Constants::Actions::ScriptEditor));
    QObject::connect(showEditor, &QAction::triggered, this, &ViewMenu::openScriptEditor);

    auto* showNowPlaying = new QAction(tr("Show playing &track"), this);
    showNowPlaying->setStatusTip(tr("Show the currently playing track in the playlist"));
    viewMenu->addAction(m_actionManager->registerAction(showNowPlaying, Constants::Actions::ShowNowPlaying));
    QObject::connect(showNowPlaying, &QAction::triggered, this, &ViewMenu::showNowPlaying);
}
} // namespace Fooyin

#include "moc_viewmenu.cpp"
