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
#include <core/database/trackdatabase.h>
#include <gui/guiconstants.h>
#include <gui/statusevent.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/async.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>

namespace Fooyin {
LibraryMenu::LibraryMenu(Application* core, ActionManager* actionManager, QObject* parent)
    : QObject{parent}
    , m_database{core->databasePool()}
    , m_library{core->library()}
{
    auto* libraryMenu = actionManager->actionContainer(Constants::Menus::Library);

    const QStringList libraryCategory = {tr("Library")};

    auto* dbMenu = actionManager->createMenu(Constants::Menus::Database);
    dbMenu->menu()->setTitle(tr("&Database"));

    auto* cleanDb = new QAction(tr("&Clean"), this);
    cleanDb->setStatusTip(tr("Remove non-library tracks not in any playlists and expired playback statistics"));
    dbMenu->addAction(cleanDb);
    QObject::connect(cleanDb, &QAction::triggered, this, &LibraryMenu::cleanupDatabase);

    auto* optimiseDb = new QAction(tr("&Optimise"), this);
    optimiseDb->setStatusTip(tr("Reduce disk usage and improve query performance"));
    dbMenu->addAction(optimiseDb);
    QObject::connect(optimiseDb, &QAction::triggered, this, &LibraryMenu::optimiseDatabase);

    auto* removeUnavailable = new QAction(tr("Remove &unavailable tracks"), this);
    removeUnavailable->setStatusTip(tr("Remove unavailable tracks from the database and libraries"));
    dbMenu->addAction(removeUnavailable);
    QObject::connect(removeUnavailable, &QAction::triggered, this, &LibraryMenu::removeUnavailbleTracks);

    auto* refreshLibrary
        = new QAction(Utils::iconFromTheme(Constants::Icons::RescanLibrary), tr("&Scan for changes"), this);
    refreshLibrary->setStatusTip(tr("Update tracks in libraries which have been modified on disk"));
    auto* refreshLibraryCmd = actionManager->registerAction(refreshLibrary, Constants::Actions::Refresh);
    refreshLibraryCmd->setCategories(libraryCategory);
    QObject::connect(refreshLibrary, &QAction::triggered, core->library(), &MusicLibrary::refreshAll);

    auto* rescanLibrary
        = new QAction(Utils::iconFromTheme(Constants::Icons::RescanLibrary), tr("&Reload tracks"), this);
    rescanLibrary->setStatusTip(tr("Reload metadata from files for all tracks in libraries"));
    auto* rescanLibraryCmd = actionManager->registerAction(rescanLibrary, Constants::Actions::Rescan);
    rescanLibraryCmd->setCategories(libraryCategory);
    QObject::connect(rescanLibrary, &QAction::triggered, core->library(), &MusicLibrary::rescanAll);

    auto* search = new QAction(tr("S&earch"), this);
    search->setStatusTip(tr("Search all libraries"));
    auto* searchCmd = actionManager->registerAction(search, Constants::Actions::SearchLibrary);
    searchCmd->setCategories(libraryCategory);
    QObject::connect(search, &QAction::triggered, this, &LibraryMenu::requestSearch);

    auto* quickSearch = new QAction(tr("&Quick Search"), this);
    quickSearch->setStatusTip(tr("Show quick search popup"));
    auto* quickSearchCmd = actionManager->registerAction(quickSearch, Constants::Actions::QuickSearch);
    quickSearchCmd->setCategories(libraryCategory);
    QObject::connect(quickSearch, &QAction::triggered, this, &LibraryMenu::requestQuickSearch);

    auto* openSettings = new QAction(Utils::iconFromTheme(Constants::Icons::Settings), tr("&Configure"), this);
    openSettings->setStatusTip(tr("Open the library page in the settings dialog"));
    auto* openSettingsCmd = actionManager->registerAction(openSettings, "Library.Configure");
    openSettingsCmd->setCategories(libraryCategory);
    QObject::connect(openSettings, &QAction::triggered, this, [core]() {
        core->settingsManager()->settingsDialog()->openAtPage(Constants::Page::LibraryGeneral);
    });

    libraryMenu->addAction(refreshLibraryCmd);
    libraryMenu->addAction(rescanLibraryCmd);
    libraryMenu->addSeparator();
    libraryMenu->addMenu(dbMenu);
    libraryMenu->addAction(searchCmd);
    libraryMenu->addAction(quickSearchCmd);
    libraryMenu->addSeparator();
    libraryMenu->addAction(openSettingsCmd);
}

void LibraryMenu::removeUnavailbleTracks()
{
    auto* progress
        = new ElapsedProgressDialog(tr("Removing unavailable tracks…"), tr("Abort"), 0, 1, Utils::getMainWindow());
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setValue(0);
    progress->setShowRemaining(false);
    progress->setWindowTitle(tr("Removing unavailable tracks"));

    m_deleteRequest = m_library->removeUnavailbleTracks();
    QObject::connect(
        m_library, &MusicLibrary::tracksDeleted, this,
        [this, progress]() {
            progress->setValue(1);
            progress->deleteLater();
            m_deleteRequest = {};
        },
        Qt::SingleShotConnection);

    QObject::connect(progress, &ElapsedProgressDialog::cancelled, progress, [this, progress]() {
        if(m_deleteRequest.cancel) {
            m_deleteRequest.cancel();
        }
        m_deleteRequest = {};
        progress->deleteLater();
    });
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

void LibraryMenu::cleanupDatabase()
{
    StatusEvent::post(tr("Cleaning database…"), 0);

    Utils::asyncExec([this]() {
        const DbConnectionHandler dbHandler{m_database};
        const DbConnectionProvider dbProvider{m_database};

        TrackDatabase trackDb;
        trackDb.initialise(dbProvider);

        trackDb.cleanupTracks();
    }).then(this, []() { StatusEvent::post(tr("Database cleaned")); });
}
} // namespace Fooyin

#include "moc_librarymenu.cpp"
