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

#include "playlistwidget.h"

#include "gui/guisettings.h"
#include "playlistdelegate.h"
#include "playlistmodel.h"
#include "playlistview.h"

#include <core/library/librarymanager.h>
#include <core/library/musiclibrary.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlisthandler.h>

#include <utils/actions/actioncontainer.h>
#include <utils/headerview.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <QToolBar>

namespace Fy::Gui::Widgets {
PlaylistWidget::PlaylistWidget(Core::Library::MusicLibrary* library, Core::Playlist::PlaylistHandler* playlistHandler,
                               Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                               QWidget* parent)
    : FyWidget{parent}
    , m_library{library}
    , m_playerManager{playerManager}
    , m_settings{settings}
    , m_settingsDialog{settings->settingsDialog()}
    , m_playlistHandler{playlistHandler}
    , m_layout{new QHBoxLayout(this)}
    , m_model{new PlaylistModel(m_playerManager, m_playlistHandler, m_settings, this)}
    , m_playlist{new PlaylistView(this)}
    , m_header{new Utils::HeaderView(Qt::Horizontal, this)}
    , m_changingSelection{false}
{
    setObjectName("Playlist");
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_header->setStretchLastSection(true);
    m_playlist->setHeader(m_header);

    m_playlist->setModel(m_model);
    m_playlist->setItemDelegate(new PlaylistDelegate(this));

    m_layout->addWidget(m_playlist);

    reset();
    setHeaderHidden(m_settings->value<Settings::PlaylistHeader>());
    setScrollbarHidden(m_settings->value<Settings::PlaylistScrollBar>());
    setupConnections();
}

void PlaylistWidget::reset()
{
    m_playlist->expandAll();
    m_playlist->scrollToTop();
}

void PlaylistWidget::setupConnections()
{
    connect(m_library, &Core::Library::MusicLibrary::tracksLoaded, m_model, &PlaylistModel::setupModelData);
    connect(m_library, &Core::Library::MusicLibrary::tracksSorted, m_model, &PlaylistModel::reset);
    connect(m_library, &Core::Library::MusicLibrary::tracksDeleted, m_model, &PlaylistModel::reset);
    connect(m_library, &Core::Library::MusicLibrary::tracksUpdated, m_model, &PlaylistModel::reset);
    connect(m_library, &Core::Library::MusicLibrary::tracksAdded, m_model, &PlaylistModel::setupModelData);
    connect(m_library, &Core::Library::MusicLibrary::libraryRemoved, m_model, &PlaylistModel::reset);
    connect(m_library, &Core::Library::MusicLibrary::libraryChanged, m_model, &PlaylistModel::reset);

    m_settings->subscribe<Settings::PlaylistHeader>(this, &PlaylistWidget::setHeaderHidden);
    m_settings->subscribe<Settings::PlaylistScrollBar>(this, &PlaylistWidget::setScrollbarHidden);

    connect(m_model, &QAbstractItemModel::modelReset, this, &PlaylistWidget::reset);
    connect(m_model, &QAbstractItemModel::modelAboutToBeReset, m_playlist, &QAbstractItemView::clearSelection);

    connect(m_playlist->header(),
            &QHeaderView::customContextMenuRequested,
            this,
            &PlaylistWidget::customHeaderMenuRequested);
    connect(
        m_playlist->selectionModel(), &QItemSelectionModel::selectionChanged, this, &PlaylistWidget::selectionChanged);
    connect(m_playlist, &PlaylistView::doubleClicked, this, &PlaylistWidget::playTrack);

    connect(m_playerManager, &Core::Player::PlayerManager::playStateChanged, this, &PlaylistWidget::changeState);
    connect(
        m_playerManager, &Core::Player::PlayerManager::currentTrackChanged, m_model, &PlaylistModel::changeTrackState);
    connect(
        m_playlistHandler, &Core::Playlist::PlaylistHandler::currentPlaylistChanged, m_model, &PlaylistModel::reset);

    connect(m_model, &QAbstractItemModel::rowsInserted, this, &PlaylistWidget::expandPlaylist);

    connect(m_header, &Utils::HeaderView::leftClicked, this, &PlaylistWidget::switchContextMenu);
}

bool PlaylistWidget::isHeaderHidden()
{
    return m_playlist->isHeaderHidden();
}

void PlaylistWidget::setHeaderHidden(bool showHeader)
{
    m_playlist->setHeaderHidden(!showHeader);
}

bool PlaylistWidget::isScrollbarHidden()
{
    return m_playlist->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
}

void PlaylistWidget::setScrollbarHidden(bool showScrollBar)
{
    m_playlist->setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

QString PlaylistWidget::name() const
{
    return "Playlist";
}

void PlaylistWidget::selectionChanged()
{
    if(m_changingSelection) {
        return;
    }
    m_changingSelection = true;
    clearSelection();

    const auto selectedIndexes = m_playlist->selectionModel()->selectedIndexes();
    std::vector<QModelIndex> indexes;

    indexes.insert(indexes.cend(), selectedIndexes.cbegin(), selectedIndexes.cend());

    while(!indexes.empty()) {
        const auto index = indexes.front();
        indexes.erase(indexes.cbegin());
        if(index.isValid()) {
            const auto type = index.data(Playlist::Type).value<PlaylistItem::Type>();
            if(type == PlaylistItem::Track) {
                m_selectedTracks.emplace_back(index.data(PlaylistItem::Role::Data).value<Core::Track>());
            }
            else {
                const QItemSelection selectedChildren{m_model->index(0, 0, index),
                                                      m_model->index(m_model->rowCount(index) - 1, 0, index)};
                const auto childIndexes = selectedChildren.indexes();
                indexes.insert(indexes.end(), childIndexes.begin(), childIndexes.end());
                m_playlist->selectionModel()->select(selectedChildren, QItemSelectionModel::Select);
            }
        }
    }
    emit selectionWasChanged(m_selectedTracks);
    m_changingSelection = false;
}

void PlaylistWidget::keyPressEvent(QKeyEvent* e)
{
    const auto key = e->key();

    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        //        if(!m_selectedTracks.empty()) {
        //            const int idx = m_model->findTrackIndex(m_selectedTracks.front());
        //            emit clickedTrack(idx);

        //            m_model->changeTrackState();
        //            m_playlist->clearSelection();
        //            m_selectedTracks.clear();
        //        }
    }
    QWidget::keyPressEvent(e);
}

// void PlaylistWidget::spanHeaders(QModelIndex parent)
//{
//     for (int i = 0; i < m_model->rowCount(parent); i++)
//     {
//         auto idx = m_model->index(i, 0, parent);
//         if (m_model->hasChildren(idx))
//         {
//             m_playlist->setFirstColumnSpanned(i, parent, true);
//             spanHeaders(idx);
//         }
//     }
// }

void PlaylistWidget::customHeaderMenuRequested(QPoint pos)
{
    Q_UNUSED(pos)
}

void PlaylistWidget::changeState(Core::Player::PlayState state)
{
    switch(state) {
        case(Core::Player::Playing):
            m_model->changeTrackState();
            return findCurrent();
        case(Core::Player::Stopped):
        case(Core::Player::Paused):
            return m_model->changeTrackState();
    }
}

void PlaylistWidget::playTrack(const QModelIndex& index)
{
    const auto type = index.data(Playlist::Type).value<PlaylistItem::Type>();
    Core::Track track;

    if(type != PlaylistItem::Track && !m_selectedTracks.empty()) {
        track = m_selectedTracks.front();
    }
    else {
        track = index.data(PlaylistItem::Role::Data).value<Core::Track>();
    }

    m_playlistHandler->changeCurrentTrack(track);
    m_model->changeTrackState();
    clearSelection(true);
}

void PlaylistWidget::findCurrent()
{
    const Core::Track track = m_playerManager->currentTrack();
    QModelIndex index       = m_model->indexForTrack(track);
    if(index.isValid()) {
        m_playlist->scrollTo(index);
        m_playlist->setCurrentIndex(index);
    }
}

void PlaylistWidget::expandPlaylist(const QModelIndex& parent, int first, int last)
{
    while(first <= last) {
        m_playlist->expand(m_model->index(first, 0, parent));
        ++first;
    }
}

void PlaylistWidget::clearSelection(bool clearView)
{
    if(clearView) {
        m_playlist->clearSelection();
    }
    m_selectedTracks.clear();
}

void PlaylistWidget::switchContextMenu(int section, QPoint pos)
{
    Q_UNUSED(section)
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    //    const auto& activePlaylist = m_playlistHandler->activePlaylist();
    const auto& playlists = m_playlistHandler->playlists();

    for(const auto& playlist : playlists) {
        auto* switchtoPlaylist = new QAction(playlist->name(), menu);
        const int id           = playlist->id();
        QObject::connect(switchtoPlaylist, &QAction::triggered, this, [this, id]() {
            m_playlistHandler->changeCurrentPlaylist(id);
        });
        menu->addAction(switchtoPlaylist);
    }
    menu->popup(mapToGlobal(pos));
}
} // namespace Fy::Gui::Widgets
