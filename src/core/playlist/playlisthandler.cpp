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

#include "database/database.h"
#include "database/playlistdatabase.h"

#include <core/coresettings.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlist.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <ranges>

namespace {
enum class CommonOperation
{
    Update,
    Remove
};

bool updateCommonTracks(Fy::Core::TrackList& tracks, const Fy::Core::TrackList& updatedTracks,
                        CommonOperation operation)
{
    Fy::Core::TrackList result;
    result.reserve(tracks.size());
    bool haveCommonTracks{false};

    for(const Fy::Core::Track& track : tracks) {
        auto it = std::ranges::find_if(std::as_const(updatedTracks), [&](const Fy::Core::Track& updatedTrack) {
            return track.id() == updatedTrack.id();
        });
        if(it != updatedTracks.end()) {
            haveCommonTracks = true;
            if(operation == CommonOperation::Update) {
                result.push_back(*it);
            }
        }
        else {
            result.push_back(track);
        }
    }

    tracks = result;
    return haveCommonTracks;
}
} // namespace

namespace Fy::Core::Playlist {
struct PlaylistHandler::Private
{
    PlaylistHandler* self;

    Player::PlayerManager* playerManager;
    Utils::SettingsManager* settings;
    DB::PlaylistDatabase playlistConnector;

    PlaylistList playlists;
    PlaylistList removedPlaylists;
    Playlist* activePlaylist{nullptr};

    Private(PlaylistHandler* self, DB::Database* database, Player::PlayerManager* playerManager,
            Utils::SettingsManager* settings)
        : self{self}
        , playerManager{playerManager}
        , settings{settings}
        , playlistConnector{database->connectionName()}
    { }

    void startNextTrack(const Track& track, int index) const
    {
        playerManager->changeCurrentTrack(track);
        QMetaObject::invokeMethod(self, "activeTrackChanged", Q_ARG(const Track&, track), Q_ARG(int, index));
        playerManager->play();
    }

    void nextTrack(int delta) const
    {
        if(!activePlaylist) {
            return;
        }

        const Track nextTrack = activePlaylist->nextTrack(playerManager->playMode(), delta);

        if(!nextTrack.isValid()) {
            playerManager->stop();
            return;
        }

        startNextTrack(nextTrack, activePlaylist->currentTrackIndex());
    }

    void next() const
    {
        nextTrack(1);
    }

    void previous() const
    {
        if(settings->value<Settings::RewindPreviousTrack>() && playerManager->currentPosition() > 5000) {
            playerManager->changePosition(0);
        }
        else {
            nextTrack(-1);
        }
    }

    void updateIndices()
    {
        for(int index{0}; auto& playlist : playlists) {
            playlist->setIndex(index++);
        }
    }

    [[nodiscard]] int nameCount(const QString& name) const
    {
        if(name.isEmpty()) {
            return -1;
        }
        return static_cast<int>(std::ranges::count_if(std::as_const(playlists), [name](const auto& playlist) {
            return QString::compare(name, playlist->name(), Qt::CaseInsensitive) == 0;
        }));
    }

    [[nodiscard]] QString findUniqueName(const QString& name) const
    {
        QString newName{name};
        int count{1};
        while(nameCount(newName) >= 1) {
            newName = name + " (" + QString::number(count) + ")";
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
            return playlist->name().compare(name, Qt::CaseInsensitive) == 0;
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

        auto indices
            = playlists | std::ranges::views::transform([](const auto& playlist) { return playlist->index(); });
        auto maxIndex = std::ranges::max_element(indices);

        const int nextValidIndex = *maxIndex + 1;
        return nextValidIndex;
    }

    [[nodiscard]] bool validId(int id) const
    {
        auto it = std::ranges::find_if(std::as_const(playlists),
                                       [id](const auto& playlist) { return playlist->id() == id; });
        return it != playlists.cend();
    }

    [[nodiscard]] bool validName(const QString& name) const
    {
        auto it = std::ranges::find_if(std::as_const(playlists),
                                       [name](const auto& playlist) { return playlist->name() == name; });
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
        index        = nextValidIndex();
        const int id = playlistConnector.insertPlaylist(name, index);
        if(id >= 0) {
            auto* playlist = playlists.emplace_back(std::make_unique<Playlist>(id, name, index)).get();
            return playlist;
        }
        return nullptr;
    }
};

PlaylistHandler::PlaylistHandler(DB::Database* database, Player::PlayerManager* playerManager,
                                 Utils::SettingsManager* settings, QObject* parent)
    : PlaylistManager{parent}
    , p{std::make_unique<Private>(this, database, playerManager, settings)}
{
    p->playlists.clear();
    p->playlistConnector.getAllPlaylists(p->playlists);

    if(p->playlists.empty()) {
        PlaylistHandler::createPlaylist(QStringLiteral("Default"), {});
    }

    QObject::connect(p->playerManager, &Player::PlayerManager::nextTrack, this, [this]() { p->next(); });
    QObject::connect(p->playerManager, &Player::PlayerManager::previousTrack, this, [this]() { p->previous(); });
}

PlaylistHandler::~PlaylistHandler()
{
    p->playlists.clear();
}

Playlist* PlaylistHandler::playlistById(int id) const
{
    auto playlist = std::ranges::find_if(std::as_const(p->playlists),
                                         [id](const auto& playlist) { return playlist->id() == id; });
    if(playlist != p->playlists.cend()) {
        return playlist->get();
    }
    return {};
}

Playlist* PlaylistHandler::playlistByIndex(int index) const
{
    auto playlist = std::ranges::find_if(std::as_const(p->playlists),
                                         [index](const auto& playlist) { return playlist->index() == index; });
    if(playlist != p->playlists.cend()) {
        return playlist->get();
    }
    return {};
}

Playlist* PlaylistHandler::playlistByName(const QString& name) const
{
    auto playlist = std::ranges::find_if(std::as_const(p->playlists),
                                         [name](const auto& playlist) { return playlist->name() == name; });
    if(playlist != p->playlists.cend()) {
        return playlist->get();
    }
    return {};
}

const PlaylistList& PlaylistHandler::playlists() const
{
    return p->playlists;
}

Playlist* PlaylistHandler::createPlaylist(const QString& name, const TrackList& tracks)
{
    const bool isNew = p->indexFromName(name) < 0;
    auto* playlist   = p->addNewPlaylist(name);
    if(playlist) {
        playlist->replaceTracks(tracks);
        if(isNew) {
            emit playlistAdded(playlist);
        }
    }
    return playlist;
}

void PlaylistHandler::appendToPlaylist(int id, const TrackList& tracks)
{
    if(auto* playlist = playlistById(id)) {
        playlist->appendTracks(tracks);
        emit playlistTracksChanged(playlist);
    }
}

void PlaylistHandler::changePlaylistIndex(int id, int index)
{
    if(auto* playlist = playlistById(id)) {
        Utils::move(p->playlists, playlist->index(), index);
        p->updateIndices();
    }
}

void PlaylistHandler::createEmptyPlaylist()
{
    const QString name = p->findUniqueName(QStringLiteral("Playlist"));
    createPlaylist(name, {});
}

void PlaylistHandler::changeActivePlaylist(int id)
{
    auto playlist = std::ranges::find_if(std::as_const(p->playlists),
                                         [id](const auto& playlist) { return playlist->id() == id; });
    if(playlist != p->playlists.cend()) {
        p->activePlaylist = playlist->get();
        emit activePlaylistChanged(playlist->get());
    }
}

void PlaylistHandler::renamePlaylist(int id, const QString& name)
{
    if(playlistCount() < 1) {
        return;
    }
    auto* playlist = playlistById(id);
    if(!playlist) {
        qDebug() << "Playlist " + QString::number(id) + " could not be found";
        return;
    }
    const QString newName = p->findUniqueName(name);
    if(!p->playlistConnector.renamePlaylist(playlist->id(), newName)) {
        qDebug() << "Playlist " + QString::number(id) + " could not be renamed to " + name;
        return;
    }
    playlist->setName(newName);

    emit playlistRenamed(playlist);
}

void PlaylistHandler::removePlaylist(int id)
{
    auto* playlist = playlistById(id);
    if(!playlist) {
        return;
    }

    if(!p->playlistConnector.removePlaylist(id)) {
        return;
    }

    if(playlist == p->activePlaylist) {
        p->activePlaylist = nullptr;
        emit activePlaylistChanged({});
    }

    const int index = p->indexFromName(playlist->name());
    p->removedPlaylists.push_back(std::move(p->playlists.at(index)));
    p->playlists.erase(p->playlists.begin() + index);

    p->updateIndices();

    if(p->playlists.empty()) {
        createPlaylist(QStringLiteral("Default"), {});
    }

    emit playlistRemoved(playlist);
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
    p->updateIndices();

    p->playlistConnector.saveModifiedPlaylists(p->playlists);

    if(p->activePlaylist) {
        p->settings->set<Settings::ActivePlaylistId>(p->activePlaylist->id());
    }
}

void PlaylistHandler::startPlayback(int playlistId)
{
    if(!p->validId(playlistId)) {
        return;
    }
    if(auto* playlist = playlistById(playlistId)) {
        changeActivePlaylist(playlistId);
        p->startNextTrack(playlist->currentTrack(), playlist->currentTrackIndex());
    }
}

void PlaylistHandler::populatePlaylists(const TrackList& tracks)
{
    TrackIdMap idTracks;
    for(const Track& track : tracks) {
        idTracks.emplace(track.id(), track);
    }

    p->playlistConnector.getPlaylistTracks(p->playlists, idTracks);

    const int lastId = p->settings->value<Settings::ActivePlaylistId>();
    if(lastId >= 0) {
        changeActivePlaylist(lastId);
    }

    emit playlistsPopulated();
}

void PlaylistHandler::libraryRemoved(int id)
{
    for(auto& playlist : p->playlists) {
        TrackList tracks = playlist->tracks();
        for(Track& track : tracks) {
            if(track.libraryId() == id) {
                track.setEnabled(false);
            }
        }
        playlist->replaceTracks(tracks);
    }
}

void PlaylistHandler::tracksUpdated(const TrackList& tracks)
{
    for(auto& playlist : p->playlists) {
        TrackList playlistTracks = playlist->tracks();
        if(updateCommonTracks(playlistTracks, tracks, CommonOperation::Update)) {
            playlist->replaceTracks(playlistTracks);
            emit playlistTracksChanged(playlist.get());
        }
    }
}

void PlaylistHandler::tracksRemoved(const TrackList& tracks)
{
    for(auto& playlist : p->playlists) {
        TrackList playlistTracks = playlist->tracks();
        if(updateCommonTracks(playlistTracks, tracks, CommonOperation::Remove)) {
            playlist->replaceTracks(playlistTracks);
            emit playlistTracksChanged(playlist.get());
        }
    }
}
} // namespace Fy::Core::Playlist
