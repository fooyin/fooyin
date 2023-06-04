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

#include "librarytreewidget.h"

#include "gui/guisettings.h"
#include "gui/playlist/playlistcontroller.h"
#include "librarytreemodel.h"

#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>

#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

namespace Fy::Gui::Widgets {
constexpr auto AutoPlaylist = "Library Selection";

LibraryTreeWidget::LibraryTreeWidget(Core::Library::MusicLibrary* library,
                                     Core::Playlist::PlaylistHandler* playlistHandler,
                                     Playlist::PlaylistController* playlistController, Utils::SettingsManager* settings,
                                     QWidget* parent)
    : FyWidget{parent}
    , m_library{library}
    , m_playlistHandler{playlistHandler}
    , m_playlistController{playlistController}
    , m_settings{settings}
    , m_layout{new QVBoxLayout(this)}
    , m_libraryTree{new QTreeView(this)}
    , m_model{new LibraryTreeModel(this)}
{
    setObjectName(LibraryTreeWidget::name());

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_libraryTree->setUniformRowHeights(true);
    m_libraryTree->setModel(m_model);
    m_libraryTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_layout->addWidget(m_libraryTree);

    m_model->setGroupScript(m_settings->value<Settings::LibraryTreeGrouping>());
    m_settings->subscribe<Settings::LibraryTreeGrouping>(this, &LibraryTreeWidget::groupingChanged);

    reset();

    QObject::connect(m_libraryTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &LibraryTreeWidget::selectionChanged);

    QObject::connect(m_library, &Core::Library::MusicLibrary::tracksLoaded, this, &LibraryTreeWidget::reset);
    QObject::connect(m_library, &Core::Library::MusicLibrary::tracksSorted, this, &LibraryTreeWidget::reset);
    QObject::connect(m_library, &Core::Library::MusicLibrary::tracksDeleted, this, &LibraryTreeWidget::reset);
    QObject::connect(m_library, &Core::Library::MusicLibrary::tracksAdded, this, &LibraryTreeWidget::reset);
    QObject::connect(m_library, &Core::Library::MusicLibrary::libraryRemoved, this, &LibraryTreeWidget::reset);
    QObject::connect(m_library, &Core::Library::MusicLibrary::libraryChanged, this, &LibraryTreeWidget::reset);
}

QString LibraryTreeWidget::name() const
{
    return "Library Tree";
}

QString LibraryTreeWidget::layoutName() const
{
    return "LibraryTree";
}

void LibraryTreeWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const QModelIndexList selected = m_libraryTree->selectionModel()->selection().indexes();
    if(selected.empty()) {
        return;
    }

    QStringList playlistName;
    Core::TrackList tracks;
    for(const auto& index : selected) {
        playlistName.emplace_back(index.data().toString());
        Core::TrackList indexTracks = index.data(LibraryTreeRole::Tracks).value<Core::TrackList>();
        tracks.insert(tracks.end(), indexTracks.cbegin(), indexTracks.cend());
    }

    auto* createPlaylist = new QAction("Send to playlist", menu);
    QObject::connect(createPlaylist, &QAction::triggered, this, [this, playlistName, tracks]() {
        m_playlistHandler->createPlaylist(playlistName.join(", "), tracks);
    });
    menu->addAction(createPlaylist);

    auto* addToPlaylist = new QAction("Add to current playlist", menu);
    QObject::connect(addToPlaylist, &QAction::triggered, this, [this, tracks]() {
        m_playlistHandler->appendToPlaylist(m_playlistController->currentPlaylist()->id(), tracks);
    });
    menu->addAction(addToPlaylist);

    menu->popup(mapToGlobal(event->pos()));
}

void LibraryTreeWidget::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    Q_UNUSED(selected)
    Q_UNUSED(deselected)

    const QModelIndexList selectedIndexes = m_libraryTree->selectionModel()->selectedIndexes();
    if(selectedIndexes.empty()) {
        return;
    }

    Core::TrackList tracks;
    for(const auto& index : selectedIndexes) {
        Core::TrackList indexTracks = index.data(LibraryTreeRole::Tracks).value<Core::TrackList>();
        tracks.insert(tracks.end(), indexTracks.cbegin(), indexTracks.cend());
    }
    m_playlistHandler->createPlaylist(AutoPlaylist, tracks);
}

void LibraryTreeWidget::groupingChanged(const QString& script)
{
    m_model->setGroupScript(script);
    reset();
}

void LibraryTreeWidget::reset()
{
    m_model->reload(m_library->tracks());
}

} // namespace Fy::Gui::Widgets
