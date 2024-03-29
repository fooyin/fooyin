/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include <core/playlist/playlisthandler.h>

#include "database/playlistdatabase.h"

#include <core/coresettings.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlist.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <ranges>
#include <utility>

namespace {
enum class CommonOperation : uint8_t
{
    Update = 0,
    Remove
};

std::vector<int> updateCommonTracks(Fooyin::TrackList& tracks, const Fooyin::TrackList& updatedTracks,
                                    CommonOperation operation)
{
    std::vector<int> indexes;

    Fooyin::TrackList result;
    result.reserve(tracks.size());

    for(auto trackIt{tracks.begin()}; trackIt != tracks.end(); ++trackIt) {
        auto updatedIt = std::ranges::find_if(updatedTracks, [&trackIt](const Fooyin::Track& updatedTrack) {
            return trackIt->id() == updatedTrack.id();
        });
        if(updatedIt != updatedTracks.end()) {
            indexes.push_back(static_cast<int>(std::distance(tracks.begin(), trackIt)));
            if(operation == CommonOperation::Update) {
                result.push_back(*updatedIt);
            }
        }
        else {
            result.push_back(*trackIt);
        }
    }

    tracks = result;
    return indexes;
}
} // namespace

namespace Fooyin {
struct PlaylistHandler::Private
{
    PlaylistHandler* self;

    DbConnectionPoolPtr dbPool;
    PlayerController* playerController;
    SettingsManager* settings;
    PlaylistDatabase playlistConnector;

    std::vector<std::unique_ptr<Playlist>> playlists;
    std::vector<std::unique_ptr<Playlist>> removedPlaylists;

    Playlist* activePlaylist{nullptr};
    Playlist* scheduledPlaylist{nullptr};

    Private(PlaylistHandler* self_, DbConnectionPoolPtr dbPool_, PlayerController* playerController_,
            SettingsManager* settings_)
        : self{self_}
        , dbPool{std::move(dbPool_)}
        , playerController{playerController_}
        , settings{settings_}
    {
        const DbConnectionProvider dbProvider{dbPool};
        playlistConnector.initialise(dbProvider);
    }

    void reloadPlaylists()
    {
        const std::vector<PlaylistInfo> infos = playlistConnector.getAllPlaylists();

        for(const auto& info : infos) {
            playlists.emplace_back(Playlist::create(info.dbId, info.name, info.index));
        }
    }

    void startNextTrack(const Track& track, int index) const
    {
        if(!activePlaylist) {
            return;
        }

        playerController->changeCurrentTrack({track, activePlaylist->id(), index});
        playerController->play();
    }

    void nextTrackChange(int delta)
    {
        if(scheduledPlaylist) {
            activePlaylist    = scheduledPlaylist;
            scheduledPlaylist = nullptr;
        }

        if(!activePlaylist) {
            playerController->stop();
            return;
        }

        const Track nextTrack = activePlaylist->nextTrackChange(delta, playerController->playMode());

        if(!nextTrack.isValid()) {
            playerController->stop();
            return;
        }

        startNextTrack(nextTrack, activePlaylist->currentTrackIndex());
    }

    Track nextTrack(int delta)
    {
        auto* playlist = scheduledPlaylist ? scheduledPlaylist : activePlaylist;

        if(!playlist) {
            playerController->stop();
            return {};
        }

        return activePlaylist->nextTrack(delta, playerController->playMode());
    }

    void next()
    {
        nextTrackChange(1);
    }

    void previous()
    {
        if(settings->value<Settings::Core::RewindPreviousTrack>() && playerController->currentPosition() > 5000) {
            playerController->changePosition(0);
        }
        else {
            nextTrackChange(-1);
        }
    }

    void updateIndices()
    {
        for(int index{0}; auto& playlist : playlists) {
            if(!playlist->isTemporary()) {
                playlist->setIndex(index++);
            }
        }
    }

    [[nodiscard]] QString findUniqueName(const QString& name) const
    {
        return Utils::findUniqueString(name, playlists, [](const auto& playlist) { return playlist->name(); });
    }

    [[nodiscard]] int indexFromName(const QString& name) const
    {
        if(name.isEmpty()) {
            return -1;
        }

        auto it = std::ranges::find_if(playlists, [name](const auto& playlist) {
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

    [[nodiscard]] bool validId(const Id& id) const
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

    void restoreActivePlaylist()
    {
        const int lastId = settings->value<Settings::Core::ActivePlaylistId>();
        if(lastId >= 0) {
            auto playlist = std::ranges::find_if(std::as_const(playlists),
                                                 [lastId](const auto& pl) { return pl->dbId() == lastId; });
            if(playlist != playlists.cend()) {
                activePlaylist = playlist->get();
                emit self->activePlaylistChanged(activePlaylist);
            }
        }
    }

    Playlist* addNewPlaylist(const QString& name, bool isTemporary = false)
    {
        auto existingIndex = indexFromName(name);

        if(existingIndex >= 0) {
            return playlists.at(existingIndex).get();
        }

        if(isTemporary) {
            auto* playlist = playlists.emplace_back(Playlist::create(name)).get();
            return playlist;
        }

        const int index = nextValidIndex();
        const int dbId  = playlistConnector.insertPlaylist(name, index);

        if(dbId >= 0) {
            auto* playlist = playlists.emplace_back(Playlist::create(dbId, name, index)).get();
            return playlist;
        }

        return nullptr;
    }
};

PlaylistHandler::PlaylistHandler(DbConnectionPoolPtr dbPool, PlayerController* playerController,
                                 SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, std::move(dbPool), playerController, settings)}
{
    p->reloadPlaylists();

    if(p->playlists.empty()) {
        PlaylistHandler::createPlaylist(QStringLiteral("Default"), {});
    }

    QObject::connect(p->playerController, &PlayerController::nextTrack, this, [this]() { p->next(); });
    QObject::connect(p->playerController, &PlayerController::previousTrack, this, [this]() { p->previous(); });
}

PlaylistHandler::~PlaylistHandler()
{
    p->playlists.clear();
}

Playlist* PlaylistHandler::playlistById(const Id& id) const
{
    if(!id.isValid()) {
        return nullptr;
    }

    auto playlist = std::ranges::find_if(p->playlists, [id](const auto& pl) { return pl->id() == id; });
    if(playlist != p->playlists.cend()) {
        return playlist->get();
    }

    return nullptr;
}

Playlist* PlaylistHandler::playlistByDbId(int id) const
{
    if(id < 0) {
        return nullptr;
    }

    auto playlist = std::ranges::find_if(p->playlists, [id](const auto& pl) { return pl->dbId() == id; });
    if(playlist != p->playlists.cend()) {
        return playlist->get();
    }

    return nullptr;
}

Playlist* PlaylistHandler::playlistByIndex(int index) const
{
    if(index < 0) {
        return nullptr;
    }

    auto playlist = std::ranges::find_if(p->playlists, [index](const auto& pl) { return pl->index() == index; });
    if(playlist != p->playlists.cend()) {
        return playlist->get();
    }

    return nullptr;
}

Playlist* PlaylistHandler::playlistByName(const QString& name) const
{
    auto playlist = std::ranges::find_if(p->playlists, [name](const auto& pl) { return pl->name() == name; });
    if(playlist != p->playlists.cend()) {
        return playlist->get();
    }

    return nullptr;
}

PlaylistList PlaylistHandler::playlists() const
{
    PlaylistList playlists;

    for(const auto& playlist : p->playlists) {
        if(!playlist->isTemporary()) {
            playlists.emplace_back(playlist.get());
        }
    }

    return playlists;
}

Playlist* PlaylistHandler::createEmptyPlaylist()
{
    const QString name = p->findUniqueName(QStringLiteral("Playlist"));
    return createPlaylist(name);
}

Playlist* PlaylistHandler::createTempEmptyPlaylist()
{
    const QString name = p->findUniqueName(QStringLiteral("TempPlaylist"));
    return createTempPlaylist(name);
}

Playlist* PlaylistHandler::createPlaylist(const QString& name)
{
    const bool isNew = p->indexFromName(name) < 0;
    auto* playlist   = p->addNewPlaylist(name);

    if(playlist && isNew) {
        emit playlistAdded(playlist);
    }

    return playlist;
}

Playlist* PlaylistHandler::createTempPlaylist(const QString& name)
{
    return p->addNewPlaylist(name, true);
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
        else {
            std::vector<int> changedIndexes(tracks.size());
            std::iota(changedIndexes.begin(), changedIndexes.end(), 0);
            emit playlistTracksChanged(playlist, changedIndexes);
        }
    }

    return playlist;
}

Playlist* PlaylistHandler::createTempPlaylist(const QString& name, const TrackList& tracks)
{
    auto* playlist = p->addNewPlaylist(name, true);
    if(playlist) {
        playlist->replaceTracks(tracks);
    }

    return playlist;
}

void PlaylistHandler::appendToPlaylist(const Id& id, const TrackList& tracks)
{
    if(auto* playlist = playlistById(id)) {
        const int index = playlist->trackCount();
        playlist->appendTracks(tracks);
        emit playlistTracksAdded(playlist, tracks, index);
    }
}

void PlaylistHandler::replacePlaylistTracks(const Id& id, const TrackList& tracks)
{
    if(auto* playlist = playlistById(id)) {
        playlist->replaceTracks(tracks);

        std::vector<int> changedIndexes(tracks.size());
        std::iota(changedIndexes.begin(), changedIndexes.end(), 0);

        emit playlistTracksChanged(playlist, changedIndexes);
    }
}

void PlaylistHandler::removePlaylistTracks(const Id& id, const std::vector<int>& indexes)
{
    if(auto* playlist = playlistById(id)) {
        const auto removedIndexes = playlist->removeTracks(indexes);
        emit playlistTracksRemoved(playlist, removedIndexes);
    }
}

void PlaylistHandler::clearPlaylistTracks(const Id& id)
{
    replacePlaylistTracks(id, {});
}

void PlaylistHandler::changePlaylistIndex(const Id& id, int index)
{
    if(auto* playlist = playlistById(id)) {
        if(!playlist->isTemporary()) {
            Utils::move(p->playlists, playlist->index(), index);
            p->updateIndices();
        }
    }
}

void PlaylistHandler::changeActivePlaylist(const Id& id)
{
    auto playlist = std::ranges::find_if(std::as_const(p->playlists), [id](const auto& pl) { return pl->id() == id; });
    if(playlist != p->playlists.cend()) {
        p->activePlaylist = playlist->get();
        emit activePlaylistChanged(playlist->get());
    }
}

void PlaylistHandler::changeActivePlaylist(Playlist* playlist)
{
    p->activePlaylist = playlist;
    emit activePlaylistChanged(playlist);
}

void PlaylistHandler::schedulePlaylist(const Id& id)
{
    const auto playlist
        = std::ranges::find_if(std::as_const(p->playlists), [id](const auto& pl) { return pl->id() == id; });
    if(playlist != p->playlists.cend()) {
        p->scheduledPlaylist = playlist->get();
    }
}

void PlaylistHandler::schedulePlaylist(Playlist* playlist)
{
    p->scheduledPlaylist = playlist;
}

void PlaylistHandler::clearSchedulePlaylist()
{
    p->scheduledPlaylist = nullptr;
}

Track PlaylistHandler::nextTrack()
{
    return p->nextTrack(1);
}

Track PlaylistHandler::previousTrack()
{
    return p->nextTrack(-1);
}

void PlaylistHandler::renamePlaylist(const Id& id, const QString& name)
{
    if(playlistCount() < 1) {
        return;
    }

    auto* playlist = playlistById(id);
    if(!playlist) {
        qDebug() << QStringLiteral("Playlist could not be renamed to %1").arg(name);
        return;
    }

    const QString newName = p->findUniqueName(name.isEmpty() ? QStringLiteral("Playlist") : name);

    if(!playlist->isTemporary() && !p->playlistConnector.renamePlaylist(playlist->dbId(), newName)) {
        qDebug() << QStringLiteral("Playlist could not be renamed to %1").arg(name);
        return;
    }

    playlist->setName(newName);

    emit playlistRenamed(playlist);
}

void PlaylistHandler::removePlaylist(const Id& id)
{
    auto* playlist = playlistById(id);
    if(!playlist) {
        return;
    }

    if(!playlist->isTemporary() && !p->playlistConnector.removePlaylist(playlist->dbId())) {
        return;
    }

    if(playlist == p->activePlaylist) {
        p->activePlaylist = nullptr;
        emit activePlaylistChanged({});
    }

    const int index = p->indexFromName(playlist->name());
    p->removedPlaylists.emplace_back(std::move(p->playlists.at(index)));
    p->playlists.erase(p->playlists.begin() + index);

    p->updateIndices();

    emit playlistRemoved(playlist);

    if(p->playlists.empty()) {
        createPlaylist(QStringLiteral("Default"), {});
    }
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

    PlaylistList playlistsToSave;

    for(const auto& playlist : p->playlists) {
        if(playlist->modified() || playlist->tracksModified()) {
            playlistsToSave.emplace_back(playlist.get());
        }
    }

    p->playlistConnector.saveModifiedPlaylists(playlistsToSave);

    if(p->activePlaylist && !p->activePlaylist->isTemporary()) {
        p->settings->set<Settings::Core::ActivePlaylistId>(p->activePlaylist->dbId());
    }
}

void PlaylistHandler::savePlaylist(const Id& id)
{
    if(auto* playlistToSave = playlistById(id)) {
        p->updateIndices();
        p->playlistConnector.savePlaylist(*playlistToSave);
    }
}

void PlaylistHandler::startPlayback(const Id& id)
{
    if(auto* playlist = playlistById(id)) {
        changeActivePlaylist(id);
        playlist->reset();
        p->startNextTrack(playlist->currentTrack(), playlist->currentTrackIndex());
    }
}

void PlaylistHandler::startPlayback(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    if(std::ranges::find_if(std::as_const(p->playlists), [playlist](const auto& pl) { return pl.get() == playlist; })
       == p->playlists.cend()) {
        return;
    }

    changeActivePlaylist(playlist);
    playlist->reset();
    p->startNextTrack(playlist->currentTrack(), playlist->currentTrackIndex());
}

void PlaylistHandler::populatePlaylists(const TrackList& tracks)
{
    TrackIdMap idTracks;
    for(const Track& track : tracks) {
        idTracks.emplace(track.id(), track);
    }

    for(const auto& playlist : p->playlists) {
        const TrackList playlistTracks = p->playlistConnector.getPlaylistTracks(*playlist, idTracks);
        playlist->replaceTracks(playlistTracks);
    }

    p->restoreActivePlaylist();

    emit playlistsPopulated();
}

void PlaylistHandler::tracksUpdated(const TrackList& tracks)
{
    for(auto& playlist : p->playlists) {
        TrackList playlistTracks  = playlist->tracks();
        const auto updatedIndexes = updateCommonTracks(playlistTracks, tracks, CommonOperation::Update);

        if(!updatedIndexes.empty()) {
            playlist->replaceTracks(playlistTracks);
            emit playlistTracksChanged(playlist.get(), updatedIndexes);
        }
    }
}

void PlaylistHandler::tracksRemoved(const TrackList& tracks)
{
    for(auto& playlist : p->playlists) {
        TrackList playlistTracks  = playlist->tracks();
        const auto updatedIndexes = updateCommonTracks(playlistTracks, tracks, CommonOperation::Remove);

        if(!updatedIndexes.empty()) {
            playlist->replaceTracks(playlistTracks);
            emit playlistTracksChanged(playlist.get(), updatedIndexes);
        }
    }
}

void PlaylistHandler::trackAboutToFinish()
{
    p->nextTrack(1);
}
} // namespace Fooyin

#include "core/playlist/moc_playlisthandler.cpp"
