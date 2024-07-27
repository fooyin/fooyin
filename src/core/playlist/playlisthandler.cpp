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
#include "internalcoresettings.h"
#include "library/libraryutils.h"

#include <core/coresettings.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlist.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <QLoggingCategory>

#include <ranges>
#include <utility>

Q_LOGGING_CATEGORY(PL_HANDLER, "PlaylistHandler")

constexpr auto ActiveIndex = "Player/ActivePlaylistIndex";

namespace Fooyin {
class PlaylistHandlerPrivate
{
public:
    PlaylistHandlerPrivate(PlaylistHandler* self, DbConnectionPoolPtr dbPool, std::shared_ptr<AudioLoader> audioLoader,
                           PlayerController* playerController, SettingsManager* settings);

    void reloadPlaylists();
    bool noConcretePlaylists();

    void startNextTrack(const Track& track, int index) const;
    void nextTrackChange(int delta);
    Track nextTrack(int delta);

    void next();
    void previous();

    void updateIndices();

    [[nodiscard]] QString findUniqueName(const QString& name) const;
    [[nodiscard]] int indexFromName(const QString& name) const;
    [[nodiscard]] int nextValidIndex() const;
    [[nodiscard]] bool validId(const UId& id) const;
    [[nodiscard]] bool validName(const QString& name) const;
    [[nodiscard]] bool validIndex(int index) const;

    void restoreActivePlaylist();
    Playlist* addNewPlaylist(const QString& name, bool isTemporary = false);

    PlaylistHandler* m_self;

    DbConnectionPoolPtr m_dbPool;
    std::shared_ptr<AudioLoader> m_audioLoader;
    PlayerController* m_playerController;
    SettingsManager* m_settings;
    PlaylistDatabase m_playlistConnector;

    std::vector<std::unique_ptr<Playlist>> m_playlists;
    std::vector<std::unique_ptr<Playlist>> m_removedPlaylists;

    Playlist* m_activePlaylist{nullptr};
    Playlist* m_scheduledPlaylist{nullptr};
};

PlaylistHandlerPrivate::PlaylistHandlerPrivate(PlaylistHandler* self, DbConnectionPoolPtr dbPool,
                                               std::shared_ptr<AudioLoader> audioLoader,
                                               PlayerController* playerController, SettingsManager* settings)
    : m_self{self}
    , m_dbPool{std::move(dbPool)}
    , m_audioLoader{std::move(audioLoader)}
    , m_playerController{playerController}
    , m_settings{settings}
{
    const DbConnectionProvider dbProvider{m_dbPool};
    m_playlistConnector.initialise(dbProvider);
}

void PlaylistHandlerPrivate::reloadPlaylists()
{
    const std::vector<PlaylistInfo> infos = m_playlistConnector.getAllPlaylists();

    for(const auto& info : infos) {
        m_playlists.emplace_back(Playlist::create(info.dbId, info.name, info.index));
    }
}

bool PlaylistHandlerPrivate::noConcretePlaylists()
{
    return m_playlists.empty()
        || std::ranges::all_of(m_playlists, [](const auto& playlist) { return playlist->isTemporary(); });
}

void PlaylistHandlerPrivate::startNextTrack(const Track& track, int index) const
{
    if(!m_activePlaylist) {
        return;
    }

    Track nextTrk{track};
    if(!nextTrk.metadataWasRead()) {
        if(m_audioLoader->readTrackMetadata(nextTrk)) {
            nextTrk.generateHash();
            m_activePlaylist->updateTrackAtIndex(m_activePlaylist->currentTrackIndex(), nextTrk);
        }
    }

    m_playerController->changeCurrentTrack({nextTrk, m_activePlaylist->id(), index});
    m_playerController->play();
}

void PlaylistHandlerPrivate::nextTrackChange(int delta)
{
    if(m_scheduledPlaylist) {
        m_activePlaylist    = m_scheduledPlaylist;
        m_scheduledPlaylist = nullptr;
    }

    if(!m_activePlaylist) {
        m_playerController->stop();
        return;
    }

    const Track nextTrk = m_activePlaylist->nextTrackChange(delta, m_playerController->playMode());

    if(!nextTrk.isValid()) {
        m_playerController->stop();
        return;
    }

    startNextTrack(nextTrk, m_activePlaylist->currentTrackIndex());
}

Track PlaylistHandlerPrivate::nextTrack(int delta)
{
    auto* playlist = m_scheduledPlaylist ? m_scheduledPlaylist : m_activePlaylist;

    if(!playlist) {
        m_playerController->stop();
        return {};
    }

    Track nextTrk = m_activePlaylist->nextTrack(delta, m_playerController->playMode());

    if(!nextTrk.metadataWasRead()) {
        if(m_audioLoader->readTrackMetadata(nextTrk)) {
            nextTrk.generateHash();
            const int nextIndex = m_activePlaylist->nextIndex(delta, m_playerController->playMode());
            m_activePlaylist->updateTrackAtIndex(nextIndex, nextTrk);
        }
    }

    return nextTrk;
}

void PlaylistHandlerPrivate::next()
{
    nextTrackChange(1);
}

void PlaylistHandlerPrivate::previous()
{
    if(m_settings->value<Settings::Core::RewindPreviousTrack>() && m_playerController->currentPosition() > 5000) {
        m_playerController->seek(0);
    }
    else {
        nextTrackChange(-1);
    }
}

void PlaylistHandlerPrivate::updateIndices()
{
    for(int index{0}; auto& playlist : m_playlists) {
        if(!playlist->isTemporary()) {
            playlist->setIndex(index++);
        }
    }
}

QString PlaylistHandlerPrivate::findUniqueName(const QString& name) const
{
    return Utils::findUniqueString(name, m_playlists, [](const auto& playlist) { return playlist->name(); });
}

int PlaylistHandlerPrivate::indexFromName(const QString& name) const
{
    if(name.isEmpty()) {
        return -1;
    }

    auto it = std::ranges::find_if(
        m_playlists, [name](const auto& playlist) { return playlist->name().compare(name, Qt::CaseInsensitive) == 0; });

    if(it == m_playlists.cend()) {
        return -1;
    }

    return static_cast<int>(std::ranges::distance(m_playlists.cbegin(), it));
}

int PlaylistHandlerPrivate::nextValidIndex() const
{
    if(m_playlists.empty()) {
        return 0;
    }

    auto indices  = m_playlists | std::ranges::views::transform([](const auto& playlist) { return playlist->index(); });
    auto maxIndex = std::ranges::max_element(indices);

    const int nextValidIndex = *maxIndex + 1;

    return nextValidIndex;
}

bool PlaylistHandlerPrivate::validId(const UId& id) const
{
    auto it
        = std::ranges::find_if(std::as_const(m_playlists), [id](const auto& playlist) { return playlist->id() == id; });
    return it != m_playlists.cend();
}

bool PlaylistHandlerPrivate::validName(const QString& name) const
{
    auto it = std::ranges::find_if(std::as_const(m_playlists),
                                   [name](const auto& playlist) { return playlist->name() == name; });
    return it != m_playlists.cend();
}

bool PlaylistHandlerPrivate::validIndex(int index) const
{
    return (index >= 0 && index < static_cast<int>(m_playlists.size()));
}

void PlaylistHandlerPrivate::restoreActivePlaylist()
{
    if(!m_settings->value<Settings::Core::Internal::SavePlaybackState>()) {
        return;
    }

    const int lastId = m_settings->value<Settings::Core::ActivePlaylistId>();
    if(lastId < 0) {
        return;
    }

    auto playlist
        = std::ranges::find_if(std::as_const(m_playlists), [lastId](const auto& pl) { return pl->dbId() == lastId; });
    if(playlist == m_playlists.cend()) {
        return;
    }

    m_activePlaylist = playlist->get();
    emit m_self->activePlaylistChanged(m_activePlaylist);

    const int lastIndex = m_settings->fileValue(QString::fromLatin1(ActiveIndex)).toInt();
    m_activePlaylist->changeCurrentIndex(lastIndex);
    m_playerController->changeCurrentTrack({m_activePlaylist->currentTrack(), m_activePlaylist->id(), lastIndex});
}

Playlist* PlaylistHandlerPrivate::addNewPlaylist(const QString& name, bool isTemporary)
{
    auto existingIndex = indexFromName(name);

    if(existingIndex >= 0) {
        return m_playlists.at(existingIndex).get();
    }

    if(isTemporary) {
        const QString tempName = !name.isEmpty() ? name : findUniqueName(QStringLiteral("TempPlaylist"));
        auto* playlist         = m_playlists.emplace_back(Playlist::create(tempName)).get();
        return playlist;
    }

    const QString playlistName = !name.isEmpty() ? name : findUniqueName(QStringLiteral("Playlist"));

    const int index = nextValidIndex();
    const int dbId  = m_playlistConnector.insertPlaylist(playlistName, index);

    if(dbId >= 0) {
        auto* playlist = m_playlists.emplace_back(Playlist::create(dbId, playlistName, index)).get();
        return playlist;
    }

    return nullptr;
}

PlaylistHandler::PlaylistHandler(DbConnectionPoolPtr dbPool, std::shared_ptr<AudioLoader> audioLoader,
                                 PlayerController* playerController, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<PlaylistHandlerPrivate>(this, std::move(dbPool), std::move(audioLoader), playerController,
                                                 settings)}
{
    p->reloadPlaylists();

    if(p->noConcretePlaylists()) {
        PlaylistHandler::createPlaylist(QStringLiteral("Default"), {});
    }

    QObject::connect(p->m_playerController, &PlayerController::nextTrack, this, [this]() { p->next(); });
    QObject::connect(p->m_playerController, &PlayerController::previousTrack, this, [this]() { p->previous(); });
}

PlaylistHandler::~PlaylistHandler()
{
    p->m_playlists.clear();
}

Playlist* PlaylistHandler::playlistById(const UId& id) const
{
    if(!id.isValid()) {
        return nullptr;
    }

    auto playlist = std::ranges::find_if(p->m_playlists, [id](const auto& pl) { return pl->id() == id; });
    if(playlist != p->m_playlists.cend()) {
        return playlist->get();
    }

    return nullptr;
}

Playlist* PlaylistHandler::playlistByDbId(int id) const
{
    if(id < 0) {
        return nullptr;
    }

    auto playlist = std::ranges::find_if(p->m_playlists, [id](const auto& pl) { return pl->dbId() == id; });
    if(playlist != p->m_playlists.cend()) {
        return playlist->get();
    }

    return nullptr;
}

Playlist* PlaylistHandler::playlistByIndex(int index) const
{
    if(index < 0) {
        return nullptr;
    }

    auto playlist = std::ranges::find_if(p->m_playlists, [index](const auto& pl) { return pl->index() == index; });
    if(playlist != p->m_playlists.cend()) {
        return playlist->get();
    }

    return nullptr;
}

Playlist* PlaylistHandler::playlistByName(const QString& name) const
{
    auto playlist = std::ranges::find_if(p->m_playlists, [name](const auto& pl) { return pl->name() == name; });
    if(playlist != p->m_playlists.cend()) {
        return playlist->get();
    }

    return nullptr;
}

PlaylistList PlaylistHandler::playlists() const
{
    PlaylistList playlists;

    for(const auto& playlist : p->m_playlists) {
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
            playlist->changeCurrentIndex(0);
            std::vector<int> changedIndexes(tracks.size());
            std::iota(changedIndexes.begin(), changedIndexes.end(), 0);
            emit tracksChanged(playlist, changedIndexes);
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

Playlist* PlaylistHandler::createNewPlaylist(const QString& name)
{
    const QString newName = p->findUniqueName(name);
    auto* playlist        = p->addNewPlaylist(newName);

    if(playlist) {
        emit playlistAdded(playlist);
    }

    return playlist;
}

Playlist* PlaylistHandler::createNewTempPlaylist(const QString& name)
{
    const QString newName = p->findUniqueName(name);
    return p->addNewPlaylist(newName, true);
}

Playlist* PlaylistHandler::createNewPlaylist(const QString& name, const TrackList& tracks)
{
    const QString newName = p->findUniqueName(name);
    auto* playlist        = p->addNewPlaylist(newName);

    if(playlist) {
        playlist->replaceTracks(tracks);
        emit playlistAdded(playlist);
    }

    return playlist;
}

Playlist* PlaylistHandler::createNewTempPlaylist(const QString& name, const TrackList& tracks)
{
    const QString newName = p->findUniqueName(name);
    auto* playlist        = p->addNewPlaylist(newName, true);
    if(playlist) {
        playlist->replaceTracks(tracks);
    }

    return playlist;
}

void PlaylistHandler::appendToPlaylist(const UId& id, const TrackList& tracks)
{
    if(auto* playlist = playlistById(id)) {
        const int index = playlist->trackCount();
        playlist->appendTracks(tracks);
        emit tracksAdded(playlist, tracks, index);
    }
}

void PlaylistHandler::replacePlaylistTracks(const UId& id, const TrackList& tracks)
{
    if(auto* playlist = playlistById(id)) {
        const auto size = tracks.empty() ? playlist->tracks().size() : tracks.size();

        playlist->replaceTracks(tracks);

        std::vector<int> changedIndexes(size);
        std::iota(changedIndexes.begin(), changedIndexes.end(), 0);

        emit tracksChanged(playlist, changedIndexes);
    }
}

void PlaylistHandler::movePlaylistTracks(const UId& id, const UId& replaceId)
{
    auto updateQueue = [this, &id, &replaceId]() {
        auto queueTracks = p->m_playerController->playbackQueue().tracks();
        for(auto& track : queueTracks) {
            if(track.playlistId == id) {
                track.playlistId = replaceId;
            }
        }
        p->m_playerController->replaceTracks(queueTracks);
    };

    if(auto* playlist = playlistById(id)) {
        if(auto* replacePlaylist = playlistById(replaceId)) {
            createPlaylist(replacePlaylist->name(), playlist->tracks());
            replacePlaylist->changeCurrentIndex(playlist->currentTrackIndex());

            if(p->m_activePlaylist == playlist) {
                p->m_playerController->updateCurrentTrackPlaylist(replaceId);
                changeActivePlaylist(replacePlaylist);
                updateQueue();
            }
        }
    }
}

void PlaylistHandler::removePlaylistTracks(const UId& id, const std::vector<int>& indexes)
{
    if(auto* playlist = playlistById(id)) {
        const auto removedIndexes = playlist->removeTracks(indexes);
        emit tracksRemoved(playlist, removedIndexes);
    }
}

void PlaylistHandler::clearPlaylistTracks(const UId& id)
{
    replacePlaylistTracks(id, TrackList{});
}

void PlaylistHandler::changePlaylistIndex(const UId& id, int index)
{
    if(auto* playlist = playlistById(id)) {
        if(!playlist->isTemporary()) {
            Utils::move(p->m_playlists, playlist->index(), index);
            p->updateIndices();
        }
    }
}

void PlaylistHandler::changeActivePlaylist(const UId& id)
{
    auto playlist
        = std::ranges::find_if(std::as_const(p->m_playlists), [id](const auto& pl) { return pl->id() == id; });
    if(playlist != p->m_playlists.cend()) {
        p->m_activePlaylist = playlist->get();
        emit activePlaylistChanged(playlist->get());
    }
}

void PlaylistHandler::changeActivePlaylist(Playlist* playlist)
{
    p->m_activePlaylist = playlist;
    emit activePlaylistChanged(playlist);
}

void PlaylistHandler::schedulePlaylist(const UId& id)
{
    const auto playlist
        = std::ranges::find_if(std::as_const(p->m_playlists), [id](const auto& pl) { return pl->id() == id; });
    if(playlist != p->m_playlists.cend()) {
        p->m_scheduledPlaylist = playlist->get();
    }
}

void PlaylistHandler::schedulePlaylist(Playlist* playlist)
{
    p->m_scheduledPlaylist = playlist;
}

void PlaylistHandler::clearSchedulePlaylist()
{
    p->m_scheduledPlaylist = nullptr;
}

Track PlaylistHandler::nextTrack()
{
    return p->nextTrack(1);
}

Track PlaylistHandler::previousTrack()
{
    return p->nextTrack(-1);
}

void PlaylistHandler::renamePlaylist(const UId& id, const QString& name)
{
    if(playlistCount() < 1) {
        return;
    }

    auto* playlist = playlistById(id);
    if(!playlist) {
        qCDebug(PL_HANDLER) << "Playlist could not be renamed to" << name;
        return;
    }

    const QString newName = p->findUniqueName(name.isEmpty() ? QStringLiteral("Playlist") : name);

    if(!playlist->isTemporary() && !p->m_playlistConnector.renamePlaylist(playlist->dbId(), newName)) {
        qCDebug(PL_HANDLER) << "Playlist could not be renamed to" << name;
        return;
    }

    playlist->setName(newName);

    emit playlistRenamed(playlist);
}

void PlaylistHandler::removePlaylist(const UId& id)
{
    auto* playlist = playlistById(id);
    if(!playlist) {
        return;
    }

    auto createDefaultIfEmpty = [this]() {
        if(p->noConcretePlaylists()) {
            createPlaylist(QStringLiteral("Default"), {});
        }
    };

    if(!playlist->isTemporary() && !p->m_playlistConnector.removePlaylist(playlist->dbId())) {
        createDefaultIfEmpty();
        return;
    }

    if(playlist == p->m_activePlaylist) {
        p->m_activePlaylist = nullptr;
        emit activePlaylistChanged({});
    }

    const int index = p->indexFromName(playlist->name());
    p->m_removedPlaylists.emplace_back(std::move(p->m_playlists.at(index)));
    p->m_playlists.erase(p->m_playlists.begin() + index);

    p->updateIndices();

    emit playlistRemoved(playlist);

    createDefaultIfEmpty();
}

Playlist* PlaylistHandler::activePlaylist() const
{
    return p->m_activePlaylist;
}

int PlaylistHandler::playlistCount() const
{
    return static_cast<int>(
        std::ranges::count_if(p->m_playlists, [](const auto& playlist) { return !playlist->isTemporary(); }));
}

void PlaylistHandler::savePlaylists()
{
    p->updateIndices();

    PlaylistList playlistsToSave;

    for(const auto& playlist : p->m_playlists) {
        if(playlist->modified() || playlist->tracksModified()) {
            playlistsToSave.emplace_back(playlist.get());
        }
    }

    p->m_playlistConnector.saveModifiedPlaylists(playlistsToSave);

    if(!p->m_activePlaylist) {
        return;
    }

    if(p->m_activePlaylist->isTemporary()) {
        p->m_settings->reset<Settings::Core::ActivePlaylistId>();
    }
    else {
        p->m_settings->set<Settings::Core::ActivePlaylistId>(p->m_activePlaylist->dbId());
    }

    if(!p->m_activePlaylist->isTemporary() && p->m_settings->value<Settings::Core::Internal::SavePlaybackState>()) {
        p->m_settings->fileSet(QString::fromLatin1(ActiveIndex), p->m_activePlaylist->currentTrackIndex());
    }
    else {
        p->m_settings->fileRemove(QString::fromLatin1(ActiveIndex));
    }
}

void PlaylistHandler::savePlaylist(const UId& id)
{
    if(auto* playlistToSave = playlistById(id)) {
        p->updateIndices();
        p->m_playlistConnector.savePlaylist(*playlistToSave);
    }
}

void PlaylistHandler::startPlayback(const UId& id)
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

    if(std::ranges::find_if(std::as_const(p->m_playlists), [playlist](const auto& pl) { return pl.get() == playlist; })
       == p->m_playlists.cend()) {
        return;
    }

    changeActivePlaylist(playlist);
    playlist->reset();
    if(playlist->currentTrackIndex() < 0) {
        playlist->changeCurrentIndex(0);
    }
    p->startNextTrack(playlist->currentTrack(), playlist->currentTrackIndex());
}

void PlaylistHandler::populatePlaylists(const TrackList& tracks)
{
    std::unordered_map<int, Track> idTracks;

    for(const Track& track : tracks) {
        idTracks.emplace(track.id(), track);
    }

    for(const auto& playlist : p->m_playlists) {
        const TrackList playlistTracks = p->m_playlistConnector.getPlaylistTracks(*playlist, idTracks);
        playlist->replaceTracks(playlistTracks);
    }

    p->restoreActivePlaylist();

    emit playlistsPopulated();
}

void PlaylistHandler::handleTracksChanged(const TrackList& tracks)
{
    for(auto& playlist : p->m_playlists) {
        TrackList playlistTracks  = playlist->tracks();
        const auto updatedIndexes = Utils::updateCommonTracks(playlistTracks, tracks, Utils::CommonOperation::Update);

        if(!updatedIndexes.empty()) {
            playlist->replaceTracks(playlistTracks);
            emit tracksChanged(playlist.get(), updatedIndexes);
        }
    }
}

void PlaylistHandler::handleTracksUpdated(const TrackList& tracks)
{
    for(auto& playlist : p->m_playlists) {
        TrackList playlistTracks  = playlist->tracks();
        const auto updatedIndexes = Utils::updateCommonTracks(playlistTracks, tracks, Utils::CommonOperation::Update);

        if(!updatedIndexes.empty()) {
            playlist->replaceTracks(playlistTracks);
            emit tracksUpdated(playlist.get(), updatedIndexes);
        }
    }
}

void PlaylistHandler::handleTracksRemoved(const TrackList& tracks)
{
    for(auto& playlist : p->m_playlists) {
        TrackList playlistTracks  = playlist->tracks();
        const auto updatedIndexes = Utils::updateCommonTracks(playlistTracks, tracks, Utils::CommonOperation::Remove);

        if(!updatedIndexes.empty()) {
            playlist->replaceTracks(playlistTracks);
            emit tracksChanged(playlist.get(), updatedIndexes);
        }
    }
}

void PlaylistHandler::trackAboutToFinish()
{
    p->nextTrack(1);
}
} // namespace Fooyin

#include "core/playlist/moc_playlisthandler.cpp"
