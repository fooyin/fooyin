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

#include "playlisthandler.h"

#include "core/coresettings.h"
#include "core/database/database.h"
#include "core/database/playlistdatabase.h"
#include "core/library/musiclibrary.h"
#include "core/player/playermanager.h"
#include "playlist.h"

#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

namespace Fy::Core::Playlist {
struct PlaylistHandler::Private : QObject
{
    PlaylistHandler* handler;

    DB::Database* database;
    Player::PlayerManager* playerManager;
    Core::Library::MusicLibrary* library;
    Utils::SettingsManager* settings;
    DB::Playlist* playlistConnector;

    PlaylistList playlists;
    Playlist* activePlaylist;

    Private(PlaylistHandler* handler, DB::Database* database, Player::PlayerManager* playerManager,
            Core::Library::MusicLibrary* library, Utils::SettingsManager* settings)
        : handler{handler}
        , database{database}
        , playerManager{playerManager}
        , library{library}
        , settings{settings}
        , playlistConnector{database->playlistConnector()}
    {
        connect(library, &Core::Library::MusicLibrary::tracksLoaded, this,
                &PlaylistHandler::Private::populatePlaylists);
        connect(library, &Core::Library::MusicLibrary::libraryRemoved, this, &PlaylistHandler::Private::libraryRemoved);
        connect(playerManager, &Player::PlayerManager::nextTrack, this, &PlaylistHandler::Private::next);
        connect(playerManager, &Player::PlayerManager::previousTrack, this, &PlaylistHandler::Private::previous);
    }

    void next()
    {
        if(!activePlaylist) {
            return;
        }
        int index = activePlaylist->currentTrackIndex();
        if(index < 0) {
            return playerManager->stop();
        }
        const bool isLastTrack      = index == activePlaylist->trackCount() - 1;
        const Player::PlayMode mode = playerManager->playMode();
        switch(mode) {
            case(Player::Shuffle):
                // TODO: Implement full shuffle functionality
                index = Utils::randomNumber(0, static_cast<int>(activePlaylist->trackCount() - 1));
                break;
            case(Player::Repeat):
                break;
            case(Player::RepeatAll):
                ++index;
                if(isLastTrack) {
                    index = 0;
                }
                break;
            case(Player::Default):
                index = isLastTrack ? -1 : index + 1;
        }
        if(index < 0) {
            return playerManager->stop();
        }
        activePlaylist->changeCurrentTrack(index);
        playerManager->changeCurrentTrack(activePlaylist->currentTrack());
    }

    void previous() const
    {
        if(!activePlaylist) {
            return;
        }
        if(playerManager->currentPosition() > 5000) {
            playerManager->changePosition(0);
        }
        int index = activePlaylist->currentTrackIndex();
        if(index < 0) {
            return playerManager->stop();
        }
        const bool isFirstTrack     = index == 0;
        const Player::PlayMode mode = playerManager->playMode();
        switch(mode) {
            case(Player::Shuffle):
                // TODO: Implement full shuffle functionality
                index = Utils::randomNumber(0, static_cast<int>(activePlaylist->trackCount() - 1));
                break;
            case(Player::Repeat):
                break;
            case(Player::RepeatAll):
                --index;
                if(isFirstTrack) {
                    index = activePlaylist->trackCount() - 1;
                }
                break;
            case(Player::Default):
                index = isFirstTrack ? -1 : index - 1;
        }
        if(index < 0) {
            return playerManager->stop();
        }
        activePlaylist->changeCurrentTrack(index);
        playerManager->changeCurrentTrack(activePlaylist->currentTrack());
    }

    void updateIndexes()
    {
        for(auto [playlist, end, i] = std::tuple{playlists.begin(), playlists.end(), 0}; playlist != end;
            ++playlist, ++i) {
            playlist->get()->setIndex(i);
        }
    }

    int nameCount(const QString& name) const
    {
        if(name.isEmpty()) {
            return -1;
        }
        return static_cast<int>(std::count_if(playlists.cbegin(), playlists.cend(), [name](const auto& playlist) {
            return playlist->name().contains(name);
        }));
    }

    QString findUniqueName(const QString& name) const
    {
        QString newName{name};
        int count{1};
        while(nameCount(newName) >= 1) {
            newName = QString{"%1 (%2)"}.arg(name).arg(count);
            ++count;
        }
        return newName;
    }

    [[nodiscard]] int indexFromName(const QString& name) const
    {
        if(name.isEmpty()) {
            return -1;
        }
        auto it = std::find_if(playlists.cbegin(), playlists.cend(), [name](const auto& playlist) {
            return (playlist->name().compare(name, Qt::CaseInsensitive) == 0);
        });
        if(it == playlists.cend()) {
            return -1;
        }
        return static_cast<int>(std::distance(playlists.cbegin(), it));
    }

    [[nodiscard]] bool validId(int id) const
    {
        auto it = std::find_if(playlists.cbegin(), playlists.cend(), [id](const auto& playlist) {
            return playlist->id() == id;
        });
        return it != playlists.cend();
    }

    [[nodiscard]] bool validIndex(int index) const
    {
        return (index >= 0 && index < static_cast<int>(playlists.size()));
    }

    Playlist* addNewPlaylist(const QString& name)
    {
        auto index = indexFromName(name);
        if(index >= 0) {
            return playlists.at(index).get();
        }
        index        = static_cast<int>(playlists.size());
        const int id = playlistConnector->insertPlaylist(name, index);
        if(id >= 0) {
            auto* playlist = playlists.emplace_back(std::make_unique<Playlist>(name, index, id)).get();
            return playlist;
        }
        return nullptr;
    }

    void populatePlaylists(const TrackList& tracks)
    {
        TrackIdMap idTracks;
        for(const Track& track : tracks) {
            idTracks.emplace(track.id(), track);
        }

        database->transaction();

        for(auto& playlist : playlists) {
            std::vector<int> trackIds;
            playlistConnector->getPlaylistTracks(playlist->id(), trackIds);
            TrackList playlistTracks;
            for(const int id : trackIds) {
                if(idTracks.count(id)) {
                    playlistTracks.emplace_back(idTracks.at(id));
                }
            }
            playlist->appendTracks(playlistTracks);
        }

        database->commit();

        const int lastId = settings->value<Settings::ActivePlaylistId>();
        if(lastId >= 0) {
            handler->changeActivePlaylist(lastId);
        }

        emit handler->playlistsPopulated();
    }

    void libraryRemoved(int id)
    {
        for(auto& playlist : playlists) {
            TrackList tracks = playlist->tracks();
            for(Track& track : tracks) {
                if(track.libraryId() == id) {
                    track.setEnabled(false);
                }
            }
            playlist->replaceTracks(tracks);
        }
    }
};

PlaylistHandler::PlaylistHandler(DB::Database* database, Player::PlayerManager* playerManager,
                                 Core::Library::MusicLibrary* library, Utils::SettingsManager* settings,
                                 QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, database, playerManager, library, settings)}
{
    p->playlists.clear();
    p->playlistConnector->getAllPlaylists(p->playlists);

    if(p->playlists.empty()) {
        createPlaylist("Default", {});
    }
}

PlaylistHandler::~PlaylistHandler()
{
    p->playlists.clear();
}

Playlist* PlaylistHandler::playlistById(int id) const
{
    auto playlist = std::find_if(p->playlists.cbegin(), p->playlists.cend(), [id](const auto& playlist) {
        return playlist->id() == id;
    });
    if(playlist != p->playlists.cend()) {
        return playlist->get();
    }
    return nullptr;
}

Playlist* PlaylistHandler::playlistByIndex(int index) const
{
    auto playlist = std::find_if(p->playlists.cbegin(), p->playlists.cend(), [index](const auto& playlist) {
        return playlist->index() == index;
    });
    if(playlist != p->playlists.cend()) {
        return playlist->get();
    }
    return nullptr;
}

Playlist* PlaylistHandler::playlistByName(const QString& name) const
{
    auto playlist = std::find_if(p->playlists.cbegin(), p->playlists.cend(), [name](const auto& playlist) {
        return playlist->name() == name;
    });
    if(playlist != p->playlists.cend()) {
        return playlist->get();
    }
    return nullptr;
}

const PlaylistList& PlaylistHandler::playlists() const
{
    return p->playlists;
}

Playlist* PlaylistHandler::createPlaylist(const QString& name, const TrackList& tracks)
{
    const bool isNew   = p->indexFromName(name) < 0;
    Playlist* playlist = p->addNewPlaylist(name);
    if(playlist) {
        playlist->replaceTracks(tracks);
        if(isNew) {
            emit playlistAdded(playlist);
        }
        else {
            emit playlistTracksChanged(playlist);
        }
        return playlist;
    }
    return nullptr;
}

void PlaylistHandler::createEmptyPlaylist()
{
    const QString name = p->findUniqueName("Playlist");
    createPlaylist(name, {});
}

void PlaylistHandler::changeActivePlaylist(int id)
{
    auto playlist = std::find_if(p->playlists.cbegin(), p->playlists.cend(), [id](const auto& playlist) {
        return playlist->id() == id;
    });
    if(playlist != p->playlists.cend()) {
        p->activePlaylist = playlist->get();
        emit activePlaylistChanged(p->activePlaylist);
    }
}

void PlaylistHandler::renamePlaylist(int id, const QString& name)
{
    if(playlistCount() < 1) {
        return;
    }
    Playlist* playlist = playlistById(id);
    if(!playlist) {
        qDebug() << QString{"Playlist %1 could not be renamed to %2"}.arg(id).arg("name");
        return;
    }
    const QString newName = p->findUniqueName(name);
    playlist->setName(newName);
    p->playlistConnector->renamePlaylist(playlist->id(), newName);
    emit playlistRenamed(playlist);
}

void PlaylistHandler::removePlaylist(int id)
{
    if(playlistCount() <= 1) {
        return;
    }
    auto* playlist = playlistById(id);
    if(!playlist) {
        return;
    }
    const int index = playlist->index();
    p->playlists.erase(p->playlists.cbegin() + index);
    p->playlistConnector->removePlaylist(id);
    if(!p->playlists.empty()) {
        p->activePlaylist = p->playlists.at(index == 0 ? index : index - 1).get();
        p->updateIndexes();
        emit playlistRemoved(id);
    }
    else {
        p->activePlaylist = {};
    }
    emit activePlaylistChanged(p->activePlaylist);
}

Playlist* PlaylistHandler::activePlaylist() const
{
    return p->activePlaylist;
}

int PlaylistHandler::playlistCount() const
{
    return static_cast<int>(p->playlists.size());
}

void PlaylistHandler::savePlaylists()
{
    p->updateIndexes();
    for(const auto& playlist : p->playlists) {
        if(playlist->wasModified()) {
            p->playlistConnector->insertPlaylistTracks(playlist->id(), playlist->tracks());
        }
    }
    p->settings->set<Settings::ActivePlaylistId>(p->activePlaylist->id());
}

void PlaylistHandler::startPlayback(int playlistId, const Core::Track& track)
{
    if(!p->validId(playlistId)) {
        return;
    }
    changeActivePlaylist(playlistId);
    p->activePlaylist->changeCurrentTrack(track);
    p->playerManager->changeCurrentTrack(p->activePlaylist->currentTrack());
    p->playerManager->play();
}
} // namespace Fy::Core::Playlist
