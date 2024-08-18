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

#include "librarymenu.h"

#include <core/application.h>
#include <core/library/musiclibrary.h>
#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>

namespace Fooyin {
LibraryMenu::LibraryMenu(Application* core, ActionManager* actionManager, QObject* parent)
    : QObject{parent}
{
    auto* libraryMenu = actionManager->actionContainer(Constants::Menus::Library);

    auto* refreshLibrary
        = new QAction(Utils::iconFromTheme(Constants::Icons::RescanLibrary), tr("&Scan for changes"), this);
    refreshLibrary->setStatusTip(tr("Update tracks in libraries which have been modified on disk"));
    libraryMenu->addAction(actionManager->registerAction(refreshLibrary, Constants::Actions::Refresh));
    QObject::connect(refreshLibrary, &QAction::triggered, core->library(), &MusicLibrary::refreshAll);

    auto* rescanLibrary
        = new QAction(Utils::iconFromTheme(Constants::Icons::RescanLibrary), tr("&Reload tracks"), this);
    rescanLibrary->setStatusTip(tr("Reload metadata from files for all tracks in libraries"));
    libraryMenu->addAction(actionManager->registerAction(rescanLibrary, Constants::Actions::Rescan));
    QObject::connect(rescanLibrary, &QAction::triggered, core->library(), &MusicLibrary::rescanAll);

    auto* openSettings = new QAction(Utils::iconFromTheme(Constants::Icons::Settings), tr("&Configure"), this);
    libraryMenu->addAction(actionManager->registerAction(openSettings, "Library.Configure"));
    QObject::connect(openSettings, &QAction::triggered, this, [core]() {
        core->settingsManager()->settingsDialog()->openAtPage(Constants::Page::LibraryGeneral);
    });
}
} // namespace Fooyin

#include "moc_librarymenu.cpp"
