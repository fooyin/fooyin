/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "playlistbox.h"

#include <core/playlist/playlisthandler.h>

#include "playlist/playlistcontroller.h"

#include <QComboBox>
#include <QVBoxLayout>

namespace Fooyin {
PlaylistBox::PlaylistBox(PlaylistController* playlistController, QWidget* parent)
    : FyWidget{parent}
    , m_playlistController{playlistController}
    , m_playlistHandler{m_playlistController->playlistHandler()}
    , m_playlistBox{new QComboBox(this)}
{
    QObject::setObjectName(PlaylistBox::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});

    layout->addWidget(m_playlistBox);

    QObject::connect(m_playlistBox, &QComboBox::currentIndexChanged, this, &PlaylistBox::changePlaylist);
    QObject::connect(m_playlistController, &PlaylistController::playlistsLoaded, this,
                     [this]() { currentPlaylistChanged(m_playlistController->currentPlaylist()); });
    QObject::connect(
        m_playlistController, &PlaylistController::currentPlaylistChanged, this,
        [this](const Playlist* /*prevPlaylist*/, const Playlist* playlist) { currentPlaylistChanged(playlist); });
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistAdded, this, &PlaylistBox::addPlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistRemoved, this, &PlaylistBox::removePlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistRenamed, this,
                     [this](const Playlist* playlist) { playlistRenamed(playlist); });

    setupBox();
}

QString PlaylistBox::name() const
{
    return tr("Playlist Switcher");
}

QString PlaylistBox::layoutName() const
{
    return QStringLiteral("PlaylistSwitcher");
}

void PlaylistBox::setupBox()
{
    const auto& playlists = m_playlistHandler->playlists();
    for(const auto& playlist : playlists) {
        addPlaylist(playlist);
    }
}

void PlaylistBox::addPlaylist(const Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    m_playlistBox->addItem(playlist->name(), QVariant::fromValue(playlist->id()));
}

void PlaylistBox::removePlaylist(const Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    const int count = m_playlistBox->count();

    for(int i{0}; i < count; ++i) {
        if(m_playlistBox->itemData(i).value<Id>() == playlist->id()) {
            m_playlistBox->removeItem(i);
        }
    }
}

void PlaylistBox::playlistRenamed(const Playlist* playlist) const
{
    if(!playlist) {
        return;
    }

    const int count = m_playlistBox->count();

    for(int i{0}; i < count; ++i) {
        if(m_playlistBox->itemData(i).value<Id>() == playlist->id()) {
            m_playlistBox->setItemText(i, playlist->name());
        }
    }
}

void PlaylistBox::currentPlaylistChanged(const Playlist* playlist) const
{
    if(!playlist) {
        return;
    }

    const int count = m_playlistBox->count();
    const Id id     = playlist->id();

    for(int i{0}; i < count; ++i) {
        if(m_playlistBox->itemData(i).value<Id>() == id) {
            // Prevent currentIndexChanged triggering changePlaylist
            const QSignalBlocker block{m_playlistBox};
            m_playlistBox->setCurrentIndex(i);
        }
    }
}

void PlaylistBox::changePlaylist(int index)
{
    if(index < 0 || index >= m_playlistBox->count()) {
        return;
    }

    const Id id = m_playlistBox->itemData(index).value<Id>();
    m_playlistController->changeCurrentPlaylist(id);
}
} // namespace Fooyin
