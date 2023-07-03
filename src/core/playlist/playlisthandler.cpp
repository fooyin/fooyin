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

#include <ranges>

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
    int activePlaylistId{-1};

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
        if(activePlaylistId < 0) {
            return;
        }

        auto activePlaylist = handler->playlistById(activePlaylistId);
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
        updatePlaylist(*activePlaylist);
        playerManager->changeCurrentTrack(activePlaylist->currentTrack());
    }

    void previous()
    {
        if(activePlaylistId < 0) {
            return;
        }

        if(playerManager->currentPosition() > 5000) {
            playerManager->changePosition(0);
        }

        auto activePlaylist = handler->playlistById(activePlaylistId);
        if(!activePlaylist) {
            return;
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
        updatePlaylist(*activePlaylist);
        playerManager->changeCurrentTrack(activePlaylist->currentTrack());
    }

    void updateIndices()
    {
        for(auto pl = playlists.begin(); pl != playlists.end(); ++pl) {
            const auto index = static_cast<int>(std::ranges::distance(playlists.cbegin(), pl));
            pl->setIndex(index);
        }
    }

    [[nodiscard]] int nameCount(const QString& name) const
    {
        if(name.isEmpty()) {
            return -1;
        }
        return static_cast<int>(std::ranges::count_if(std::as_const(playlists), [name](const auto& playlist) {
            return QString::compare(name, playlist.name(), Qt::CaseInsensitive) == 0;
        }));
    }

    [[nodiscard]] QString findUniqueName(const QString& name) const
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
        auto it = std::ranges::find_if(std::as_const(playlists), [name](const auto& playlist) {
            return playlist.name().compare(name, Qt::CaseInsensitive) == 0;
        });
        if(it == playlists.cend()) {
            return -1;
        }
        return static_cast<int>(std::ranges::distance(playlists.cbegin(), it));
    }

    [[nodiscard]] int nextValidIndex() const
    {
        if(playlists.empty()) {
            return 0;
        }

        auto indices  = playlists | std::ranges::views::transform([](const Playlist& playlist) {
                           return playlist.index();
                       });
        auto maxIndex = std::ranges::max_element(indices);

        const int nextIndex = *maxIndex + 1;
        return nextIndex;
    }

    [[nodiscard]] bool validId(int id) const
    {
        auto it = std::ranges::find_if(std::as_const(playlists), [id](const auto& playlist) {
            return playlist.id() == id;
        });
        return it != playlists.cend();
    }

    [[nodiscard]] bool validName(const QString& name) const
    {
        auto it = std::ranges::find_if(std::as_const(playlists), [name](const auto& playlist) {
            return playlist.name() == name;
        });
        return it != playlists.cend();
    }

    [[nodiscard]] bool validIndex(int index) const
    {
        return (index >= 0 && index < static_cast<int>(playlists.size()));
    }

    std::optional<Playlist> addNewPlaylist(const QString& name)
    {
        auto index = indexFromName(name);
        if(index >= 0) {
            return playlists.at(index);
        }
        index        = nextValidIndex();
        const int id = playlistConnector->insertPlaylist(name, index);
        if(id >= 0) {
            auto playlist = playlists.emplace_back(name, index, id);
            return playlist;
        }
        return {};
    }

    void populatePlaylists(const TrackList& tracks)
    {
        TrackIdMap idTracks;
        for(const Track& track : tracks) {
            idTracks.emplace(track.id(), track);
        }

        database->transaction();

        auto populatePlaylist = [&](Playlist& playlist) {
            std::vector<int> trackIds;
            playlistConnector->getPlaylistTracks(playlist.id(), trackIds);
            TrackList playlistTracks;
            for(const int id : trackIds) {
                if(idTracks.contains(id)) {
                    playlistTracks.push_back(idTracks.at(id));
                }
            }
            playlist.appendTracks(playlistTracks);
        };

        std::ranges::for_each(playlists, populatePlaylist);

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
            TrackList tracks = playlist.tracks();
            for(Track& track : tracks) {
                if(track.libraryId() == id) {
                    track.setEnabled(false);
                }
            }
            playlist.replaceTracks(tracks);
        }
    }

    void updatePlaylist(const Playlist& playlist)
    {
        std::ranges::replace_if(
            playlists,
            [playlist](const Playlist& pl) {
                return pl.id() == playlist.id();
            },
            playlist);
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

std::optional<Playlist> PlaylistHandler::playlistById(int id) const
{
    auto playlist = std::ranges::find_if(std::as_const(p->playlists), [id](const auto& playlist) {
        return playlist.id() == id;
    });
    if(playlist != p->playlists.cend()) {
        return *playlist;
    }
    return {};
}

std::optional<Playlist> PlaylistHandler::playlistByIndex(int index) const
{
    auto playlist = std::ranges::find_if(std::as_const(p->playlists), [index](const auto& playlist) {
        return playlist.index() == index;
    });
    if(playlist != p->playlists.cend()) {
        return *playlist;
    }
    return {};
}

std::optional<Playlist> PlaylistHandler::playlistByName(const QString& name) const
{
    auto playlist = std::ranges::find_if(std::as_const(p->playlists), [name](const auto& playlist) {
        return playlist.name() == name;
    });
    if(playlist != p->playlists.cend()) {
        return *playlist;
    }
    return {};
}

PlaylistList PlaylistHandler::playlists() const
{
    return p->playlists;
}

std::optional<Playlist> PlaylistHandler::createPlaylist(const QString& name, const TrackList& tracks, bool switchTo)
{
    const bool isNew = p->indexFromName(name) < 0;
    auto playlist    = p->addNewPlaylist(name);
    if(playlist) {
        playlist->replaceTracks(tracks);
        p->updatePlaylist(*playlist);
        if(isNew) {
            emit playlistAdded(*playlist, switchTo);
        }
        else {
            emit playlistTracksChanged(*playlist, switchTo);
        }
    }
    return playlist;
}

void PlaylistHandler::appendToPlaylist(int id, const TrackList& tracks)
{
    if(auto playlist = playlistById(id)) {
        playlist->appendTracks(tracks);
        p->updatePlaylist(*playlist);
        emit playlistTracksChanged(*playlist);
    }
}

void PlaylistHandler::createEmptyPlaylist(bool switchTo)
{
    const QString name = p->findUniqueName("Playlist");
    createPlaylist(name, {}, switchTo);
}

void PlaylistHandler::exchangePlaylist(Playlist& playlist, const Playlist& other)
{
    playlist.changeCurrentTrack(other.currentTrackIndex());
    playlist.replaceTracks(other.tracks());
    p->updatePlaylist(playlist);
}

void PlaylistHandler::changeActivePlaylist(int id)
{
    auto playlist = std::ranges::find_if(std::as_const(p->playlists), [id](const auto& playlist) {
        return playlist.id() == id;
    });
    if(playlist != p->playlists.cend()) {
        p->activePlaylistId = playlist->id();
        emit activePlaylistChanged(*playlist);
    }
}

void PlaylistHandler::renamePlaylist(int id, const QString& name)
{
    if(playlistCount() < 1) {
        return;
    }
    auto playlist = playlistById(id);
    if(!playlist) {
        qDebug() << QString{"Playlist %1 could not be found"}.arg(id);
        return;
    }
    const QString newName = p->findUniqueName(name);
    if(!p->playlistConnector->renamePlaylist(playlist->id(), newName)) {
        qDebug() << QString{"Playlist %1 could not be renamed to %2"}.arg(id).arg("name");
        return;
    }
    playlist->setName(newName);
    p->updatePlaylist(*playlist);

    emit playlistRenamed(*playlist);
}

void PlaylistHandler::removePlaylist(int id)
{
    if(playlistCount() <= 1) {
        return;
    }
    auto playlist = playlistById(id);
    if(!playlist) {
        return;
    }
    if(!p->playlistConnector->removePlaylist(id)) {
        return;
    }
    if(playlist->id() == p->activePlaylistId) {
        p->activePlaylistId = -1;
        emit activePlaylistChanged();
    }
    const int index = p->indexFromName(playlist->name());
    p->playlists.erase(p->playlists.cbegin() + index);
    emit playlistRemoved(id);
}

std::optional<Playlist> PlaylistHandler::activePlaylist() const
{
    return playlistById(p->activePlaylistId);
}

int PlaylistHandler::playlistCount() const
{
    return static_cast<int>(p->playlists.size());
}

void PlaylistHandler::savePlaylists()
{
    p->updateIndices();
    for(const auto& playlist : p->playlists) {
        if(playlist.wasModified()) {
            p->playlistConnector->insertPlaylistTracks(playlist.id(), playlist.tracks());
        }
    }
    p->settings->set<Settings::ActivePlaylistId>(p->activePlaylistId);
}

void PlaylistHandler::startPlayback(int playlistId, const Core::Track& track)
{
    if(!p->validId(playlistId)) {
        return;
    }
    if(auto playlist = playlistById(playlistId)) {
        changeActivePlaylist(playlistId);
        playlist->changeCurrentTrack(track);
        p->updatePlaylist(*playlist);
        p->playerManager->changeCurrentTrack(playlist->currentTrack());
        p->playerManager->play();
    }
}

void PlaylistHandler::startPlayback(QString playlistName, const Track& track)
{
    if(!p->validName(playlistName)) {
        return;
    }
    if(auto playlist = playlistByName(playlistName)) {
        changeActivePlaylist(playlist->id());
        playlist->changeCurrentTrack(track);
        p->updatePlaylist(*playlist);
        p->playerManager->changeCurrentTrack(playlist->currentTrack());
        p->playerManager->play();
    }
}
} // namespace Fy::Core::Playlist
