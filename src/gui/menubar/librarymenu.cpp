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

#include "gui/statusevent.h"

#include <core/application.h>
#include <core/library/musiclibrary.h>
#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/async.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QMenu>

namespace Fooyin {
LibraryMenu::LibraryMenu(Application* core, ActionManager* actionManager, QObject* parent)
    : QObject{parent}
    , m_database{core->databasePool()}
{
    auto* libraryMenu = actionManager->actionContainer(Constants::Menus::Library);

    auto* dbMenu = actionManager->createMenu(Constants::Menus::Database);
    dbMenu->menu()->setTitle(tr("&Database"));

    auto* optimiseDb = new QAction(tr("&Optimise"), this);
    optimiseDb->setStatusTip(tr("Reduce disk usage and improve query performance"));
    dbMenu->addAction(optimiseDb);
    QObject::connect(optimiseDb, &QAction::triggered, this, &LibraryMenu::optimiseDatabase);

    auto* refreshLibrary
        = new QAction(Utils::iconFromTheme(Constants::Icons::RescanLibrary), tr("&Scan for changes"), this);
    refreshLibrary->setStatusTip(tr("Update tracks in libraries which have been modified on disk"));
    QObject::connect(refreshLibrary, &QAction::triggered, core->library(), &MusicLibrary::refreshAll);

    auto* rescanLibrary
        = new QAction(Utils::iconFromTheme(Constants::Icons::RescanLibrary), tr("&Reload tracks"), this);
    rescanLibrary->setStatusTip(tr("Reload metadata from files for all tracks in libraries"));
    QObject::connect(rescanLibrary, &QAction::triggered, core->library(), &MusicLibrary::rescanAll);

    auto* openSettings = new QAction(Utils::iconFromTheme(Constants::Icons::Settings), tr("&Configure"), this);
    openSettings->setStatusTip(tr("Open the library page in the settings dialog"));
    QObject::connect(openSettings, &QAction::triggered, this, [core]() {
        core->settingsManager()->settingsDialog()->openAtPage(Constants::Page::LibraryGeneral);
    });

    libraryMenu->addAction(actionManager->registerAction(refreshLibrary, Constants::Actions::Refresh));
    libraryMenu->addAction(actionManager->registerAction(rescanLibrary, Constants::Actions::Rescan));
    libraryMenu->addSeparator();
    libraryMenu->addMenu(dbMenu);
    libraryMenu->addSeparator();
    libraryMenu->addAction(actionManager->registerAction(openSettings, "Library.Configure"));
}

void LibraryMenu::optimiseDatabase()
{
    StatusEvent::post(tr("Optimising database…"), 0);

    Utils::asyncExec([this]() {
        const DbConnectionHandler dbHandler{m_database};
        const DbConnectionProvider dbProvider{m_database};

        GeneralDatabase generalDb;
        generalDb.initialise(dbProvider);

        generalDb.optimiseDatabase();
    }).then(this, []() { StatusEvent::post(tr("Database optimised")); });
}
} // namespace Fooyin

#include "moc_librarymenu.cpp"
