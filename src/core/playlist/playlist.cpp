/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <core/playlist/playlist.h>

#include <core/coresettings.h>
#include <core/library/tracksort.h>
#include <core/scripting/scriptparser.h>
#include <core/track.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>

#include <functional>
#include <random>
#include <ranges>
#include <set>
#include <unordered_map>
#include <utility>

using namespace Qt::StringLiterals;

namespace Fooyin {
using AlbumTracks = std::vector<int>;

namespace {
int groupIndexContainingTrack(const std::vector<AlbumTracks>& groups, const int currentIndex)
{
    for(int i{0}; std::cmp_less(i, groups.size()); ++i) {
        const auto& album = groups.at(i);
        if(std::ranges::find(album, currentIndex) != album.cend()) {
            return i;
        }
    }

    return -1;
}
} // namespace

bool PlaylistTrack::isValid() const
{
    return track.isValid();
}

bool PlaylistTrack::isInPlaylist() const
{
    return playlistId.isValid();
}

bool PlaylistTrack::hasEntryId() const
{
    return entryId.isValid();
}

PlaylistTrackList PlaylistTrack::fromTracks(const TrackList& tracks, const UId& playlistId)
{
    PlaylistTrackList playlistTracks;

    for(int i{0}; const Track& track : tracks) {
        playlistTracks.emplace_back(track, playlistId, UId::create(), i++);
    }

    return playlistTracks;
}

TrackList PlaylistTrack::toTracks(const PlaylistTrackList& playlistTracks)
{
    TrackList tracks;

    for(const PlaylistTrack& track : playlistTracks) {
        tracks.emplace_back(track.track);
    }

    return tracks;
}

PlaylistTrackList PlaylistTrack::updateIndexes(const PlaylistTrackList& playlistTracks)
{
    PlaylistTrackList tracks{playlistTracks};

    for(int i{0}; PlaylistTrack& track : tracks) {
        if(!track.entryId.isValid()) {
            track.entryId = UId::create();
        }
        track.indexInPlaylist = i++;
    }

    return tracks;
}

const Track& PlaylistTrack::extractor(const PlaylistTrack& item)
{
    return item.track;
}

bool PlaylistTrack::operator<(const PlaylistTrack& other) const
{
    return std::tie(track, playlistId, entryId, indexInPlaylist)
         < std::tie(other.track, other.playlistId, other.entryId, other.indexInPlaylist);
}

bool PlaylistTrack::sameIdentityAs(const PlaylistTrack& other) const
{
    return track.sameIdentityAs(other.track);
}

struct Playlist::PrivateKey
{
    PrivateKey() { }
    PrivateKey(PrivateKey const&) = default;
};

class PlaylistPrivate
{
public:
    struct NavigationState
    {
        int currentTrackIndex{-1};
        int trackShuffleIndex{-1};
        std::vector<int> trackShuffleOrder;
        int albumShuffleIndex{-1};
        int trackInAlbumIndex{-1};
        std::vector<AlbumTracks> albumShuffleOrder;
    };

    PlaylistPrivate(int dbId, QString name, int index, SettingsManager* settings);

    [[nodiscard]] TrackList filteredAutoTracks(const TrackList& tracks);
    [[nodiscard]] TrackList updatedAutoTracks(const TrackList& tracks);

    [[nodiscard]] NavigationState navigationState() const;
    void restoreNavigationState(NavigationState state);

    void createShuffleOrder(bool anchorCurrentTrack = true);
    void createAlbumGroupOrder();
    void createAlbumShuffleOrder(bool anchorCurrentTrack = true);
    void syncShuffleStateToCurrentTrack();

    std::vector<AlbumTracks>::iterator findAlbumContainingTrack(int trackIndex);
    AlbumTracks getAlbumTracks(int currentIndex);
    std::vector<AlbumTracks> orderedGroups(const std::function<QString(const Track&)>& groupKey,
                                           const QString& sortScript = {});
    void sortAlbumTracks(AlbumTracks& album, const QString& sortScript);
    int randomTrackIndexFrom(int currentIndex) const;
    int randomAlbumIndexFrom(int currentIndex);
    int adjacentAlbumIndexFrom(int currentIndex, int delta, Playlist::PlayModes mode);

    int getShuffleIndex(int delta, Playlist::PlayModes mode, bool onlyCheck);
    int handleTrackShuffle(int delta, Playlist::PlayModes mode, bool onlyCheck);
    int handleAlbumShuffle(int delta, Playlist::PlayModes mode, bool onlyCheck);
    int handleNextAlbum(Playlist::PlayModes mode, int albumShuffleIndex, AlbumTracks& currentAlbum, int& nextIndex);
    int handlePreviousAlbum(Playlist::PlayModes mode, int albumShuffleIndex, AlbumTracks& currentAlbum, int& nextIndex);

    int getNextIndex(int delta, Playlist::PlayModes mode, bool onlyCheck);
    int getNextIndexFrom(int currentIndex, int delta, Playlist::PlayModes mode, bool onlyCheck);
    [[nodiscard]] std::optional<Track> getTrack(int index) const;
    [[nodiscard]] std::optional<PlaylistTrack> getPlaylistTrack(int index) const;
    [[nodiscard]] int indexOfTrackEntry(const UId& entryId) const;
    void replacePlaylistTracks(const PlaylistTrackList& tracks);

    UId m_id;
    int m_dbId{-1};
    QString m_name;
    int m_index{-1};
    TrackList m_tracks;
    std::vector<UId> m_trackEntryIds;

    SettingsManager* m_settings;
    ScriptParser m_parser;
    TrackSorter m_sorter;

    int m_currentTrackIndex{-1};

    int m_trackShuffleIndex{-1};
    std::vector<int> m_trackShuffleOrder;

    int m_albumShuffleIndex{-1};
    int m_trackInAlbumIndex{-1};
    std::vector<AlbumTracks> m_albumGroupOrder;
    std::vector<AlbumTracks> m_albumShuffleOrder;

    bool m_isTemporary{false};
    bool m_modified{false};
    bool m_tracksModified{false};

    bool m_isAutoPlaylist{false};
    QString m_query;
    QString m_sortQuery;
    bool m_forceSorted{false};
};

PlaylistPrivate::PlaylistPrivate(int dbId, QString name, int index, SettingsManager* settings)
    : m_id{UId::create()}
    , m_dbId{dbId}
    , m_name{std::move(name)}
    , m_index{index}
    , m_settings{settings}
{ }

TrackList PlaylistPrivate::filteredAutoTracks(const TrackList& tracks)
{
    QString query{m_query};
    if(!m_sortQuery.isEmpty()) {
        if(!query.isEmpty()) {
            query.append(u' ');
        }
        query.append(m_sortQuery);
    }

    m_parser.clearCache();
    return m_parser.filter(query, tracks);
}

TrackList PlaylistPrivate::updatedAutoTracks(const TrackList& tracks)
{
    TrackList filteredTracks = filteredAutoTracks(tracks);

    if(m_forceSorted) {
        return filteredTracks;
    }

    std::unordered_map<QString, int> filteredIndexes;
    filteredIndexes.reserve(filteredTracks.size());

    for(int i{0}; std::cmp_less(i, filteredTracks.size()); ++i) {
        filteredIndexes.emplace(filteredTracks[i].uniqueFilepath(), i);
    }

    std::vector<char> used(filteredTracks.size(), false);

    TrackList updatedTracks;
    updatedTracks.reserve(filteredTracks.size());

    for(const auto& track : m_tracks) {
        if(const auto it = filteredIndexes.find(track.uniqueFilepath()); it != filteredIndexes.end()) {
            updatedTracks.emplace_back(filteredTracks[it->second]);
            used[it->second] = true;
        }
    }

    for(size_t i{0}; i < filteredTracks.size(); ++i) {
        if(!used[i]) {
            updatedTracks.emplace_back(filteredTracks[i]);
        }
    }

    return updatedTracks;
}

PlaylistPrivate::NavigationState PlaylistPrivate::navigationState() const
{
    return {
        .currentTrackIndex = m_currentTrackIndex,
        .trackShuffleIndex = m_trackShuffleIndex,
        .trackShuffleOrder = m_trackShuffleOrder,
        .albumShuffleIndex = m_albumShuffleIndex,
        .trackInAlbumIndex = m_trackInAlbumIndex,
        .albumShuffleOrder = m_albumShuffleOrder,
    };
}

void PlaylistPrivate::restoreNavigationState(NavigationState state)
{
    m_currentTrackIndex = state.currentTrackIndex;
    m_trackShuffleIndex = state.trackShuffleIndex;
    m_trackShuffleOrder = std::move(state.trackShuffleOrder);
    m_albumShuffleIndex = state.albumShuffleIndex;
    m_trackInAlbumIndex = state.trackInAlbumIndex;
    m_albumShuffleOrder = std::move(state.albumShuffleOrder);
}

void PlaylistPrivate::createShuffleOrder(const bool anchorCurrentTrack)
{
    m_trackShuffleOrder.resize(m_tracks.size());
    std::iota(m_trackShuffleOrder.begin(), m_trackShuffleOrder.end(), 0);
    std::ranges::shuffle(m_trackShuffleOrder, std::mt19937{std::random_device{}()});

    if(anchorCurrentTrack) {
        // Move current track to start
        auto it = std::ranges::find(m_trackShuffleOrder, m_currentTrackIndex);
        if(it != m_trackShuffleOrder.end()) {
            std::rotate(m_trackShuffleOrder.begin(), it, it + 1);
        }
    }
}

void PlaylistPrivate::createAlbumGroupOrder()
{
    const QString groupScript = m_settings->value<Settings::Core::ShuffleAlbumsGroupScript>();
    const QString sortScript  = m_settings->value<Settings::Core::ShuffleAlbumsSortScript>();
    m_albumGroupOrder         = orderedGroups(
        [this, &groupScript](const Track& track) { return m_parser.evaluate(groupScript, track); }, sortScript);
}

void PlaylistPrivate::createAlbumShuffleOrder(const bool anchorCurrentTrack)
{
    if(m_albumGroupOrder.empty()) {
        createAlbumGroupOrder();
    }

    m_albumShuffleOrder = m_albumGroupOrder;
    std::ranges::shuffle(m_albumShuffleOrder, std::mt19937{std::random_device{}()});

    if(m_albumShuffleOrder.empty()) {
        m_trackInAlbumIndex = 0;
        return;
    }

    if(anchorCurrentTrack) {
        // Move the current album to the front
        auto albumIt = findAlbumContainingTrack(m_currentTrackIndex);
        if(albumIt != m_albumShuffleOrder.end()) {
            m_trackInAlbumIndex = static_cast<int>(
                std::distance(albumIt->begin(), std::find(albumIt->begin(), albumIt->end(), m_currentTrackIndex)));
            std::rotate(m_albumShuffleOrder.begin(), albumIt, albumIt + 1);
        }
        else {
            m_trackInAlbumIndex = 0;
        }
    }
    else {
        m_trackInAlbumIndex = 0;
    }
}

void PlaylistPrivate::syncShuffleStateToCurrentTrack()
{
    if(m_currentTrackIndex < 0 || std::cmp_greater_equal(m_currentTrackIndex, m_tracks.size())) {
        m_trackShuffleIndex = -1;
        m_albumShuffleIndex = -1;
        m_trackInAlbumIndex = -1;
        return;
    }

    if(!m_trackShuffleOrder.empty()) {
        const auto trackIt = std::ranges::find(m_trackShuffleOrder, m_currentTrackIndex);
        if(trackIt != m_trackShuffleOrder.cend()) {
            m_trackShuffleIndex = static_cast<int>(std::distance(m_trackShuffleOrder.begin(), trackIt));
        }
        else {
            m_trackShuffleOrder.clear();
            m_trackShuffleIndex = -1;
        }
    }

    if(!m_albumShuffleOrder.empty()) {
        const auto albumIt = findAlbumContainingTrack(m_currentTrackIndex);
        if(albumIt != m_albumShuffleOrder.end()) {
            m_albumShuffleIndex = static_cast<int>(std::distance(m_albumShuffleOrder.begin(), albumIt));
            const auto trackIt  = std::ranges::find(*albumIt, m_currentTrackIndex);
            m_trackInAlbumIndex
                = trackIt != albumIt->cend() ? static_cast<int>(std::distance(albumIt->begin(), trackIt)) : -1;
        }
        else {
            m_albumShuffleOrder.clear();
            m_albumShuffleIndex = -1;
            m_trackInAlbumIndex = -1;
        }
    }
}

std::vector<AlbumTracks>::iterator PlaylistPrivate::findAlbumContainingTrack(int trackIndex)
{
    return std::ranges::find_if(m_albumShuffleOrder, [trackIndex](const AlbumTracks& album) {
        return std::ranges::find(album, trackIndex) != album.end();
    });
}

AlbumTracks PlaylistPrivate::getAlbumTracks(int currentIndex)
{
    if(m_albumShuffleOrder.empty()) {
        createAlbumShuffleOrder();
        m_albumShuffleIndex = 0;
    }

    if(currentIndex < 0 || std::cmp_greater_equal(currentIndex, m_tracks.size())) {
        return {};
    }

    for(const auto& album : m_albumShuffleOrder) {
        const auto it = std::ranges::find(album, currentIndex);
        if(it != album.cend()) {
            return album;
        }
    }

    return {};
}

std::vector<AlbumTracks> PlaylistPrivate::orderedGroups(const std::function<QString(const Track&)>& groupKey,
                                                        const QString& sortScript)
{
    std::vector<AlbumTracks> albums;
    if(m_tracks.empty()) {
        return albums;
    }

    std::unordered_map<QString, int> albumIndexes;
    albumIndexes.reserve(m_tracks.size());

    const auto count = static_cast<int>(m_tracks.size());
    for(int i{0}; i < count; ++i) {
        const QString albumGroup = groupKey(m_tracks.at(i));
        auto [it, inserted]      = albumIndexes.try_emplace(albumGroup, static_cast<int>(albums.size()));
        if(inserted) {
            albums.emplace_back();
        }

        albums.at(it->second).emplace_back(i);
    }

    if(!sortScript.isEmpty()) {
        for(auto& albumTracks : albums) {
            sortAlbumTracks(albumTracks, sortScript);
        }
    }

    return albums;
}

void PlaylistPrivate::sortAlbumTracks(AlbumTracks& album, const QString& sortScript)
{
    PlaylistTrackList trackIndexes;
    trackIndexes.reserve(album.size());

    for(const int trackIndex : album) {
        const UId entryId
            = std::cmp_less(trackIndex, m_trackEntryIds.size()) ? m_trackEntryIds.at(trackIndex) : UId::create();
        trackIndexes.emplace_back(m_tracks.at(trackIndex), m_id, entryId, trackIndex);
    }

    trackIndexes = m_sorter.calcSortTracks(sortScript, trackIndexes, PlaylistTrack::extractor);

    album.clear();
    for(const auto& track : trackIndexes) {
        album.emplace_back(track.indexInPlaylist);
    }
}

int PlaylistPrivate::randomAlbumIndexFrom(const int currentIndex)
{
    if(m_albumShuffleOrder.empty()) {
        createAlbumShuffleOrder(false);
    }

    const auto& albums = m_albumShuffleOrder;
    if(albums.empty()) {
        return -1;
    }

    std::vector<int> candidateAlbums;
    candidateAlbums.reserve(albums.size());

    const int currentAlbumIndex = groupIndexContainingTrack(albums, currentIndex);
    for(int i{0}; std::cmp_less(i, albums.size()); ++i) {
        if(albums.at(i).empty()) {
            continue;
        }

        if(currentAlbumIndex >= 0 && albums.size() > 1 && i == currentAlbumIndex) {
            continue;
        }

        candidateAlbums.emplace_back(i);
    }

    if(candidateAlbums.empty()) {
        return albums.front().empty() ? -1 : albums.front().front();
    }

    std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution dist{0, static_cast<int>(candidateAlbums.size()) - 1};

    const auto& album = albums.at(candidateAlbums.at(dist(gen)));
    return album.empty() ? -1 : album.front();
}

int PlaylistPrivate::randomTrackIndexFrom(const int currentIndex) const
{
    const int trackCount = static_cast<int>(m_tracks.size());
    if(trackCount <= 0) {
        return -1;
    }

    const bool excludeTrack = trackCount > 1 && currentIndex >= 0 && currentIndex < trackCount;

    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution dist{0, excludeTrack ? trackCount - 2 : trackCount - 1};

    int targetIndex = dist(gen);
    if(excludeTrack && targetIndex >= currentIndex) {
        ++targetIndex;
    }

    return targetIndex;
}

int PlaylistPrivate::adjacentAlbumIndexFrom(const int currentIndex, const int delta, const Playlist::PlayModes mode)
{
    const bool useShuffleOrder = (mode & Playlist::ShuffleAlbums);
    if(useShuffleOrder && m_albumShuffleOrder.empty()) {
        createAlbumShuffleOrder(false);
    }
    if(!useShuffleOrder && m_albumGroupOrder.empty()) {
        createAlbumGroupOrder();
    }

    const auto& albums = useShuffleOrder ? m_albumShuffleOrder : m_albumGroupOrder;
    if(albums.empty()) {
        return -1;
    }

    const auto firstAlbumIndex = [albums]() {
        return albums.front().empty() ? -1 : albums.front().front();
    };
    const auto lastAlbumIndex = [albums]() {
        return albums.back().empty() ? -1 : albums.back().front();
    };

    if(currentIndex < 0 || std::cmp_greater_equal(currentIndex, m_tracks.size())) {
        return delta < 0 ? lastAlbumIndex() : firstAlbumIndex();
    }

    int targetAlbumIndex = groupIndexContainingTrack(albums, currentIndex);
    if(targetAlbumIndex < 0) {
        return delta < 0 ? lastAlbumIndex() : firstAlbumIndex();
    }

    targetAlbumIndex += delta < 0 ? -1 : 1;

    if(targetAlbumIndex < 0) {
        if(mode & Playlist::RepeatPlaylist) {
            targetAlbumIndex = static_cast<int>(albums.size()) - 1;
        }
        else {
            return -1;
        }
    }
    else if(std::cmp_greater_equal(targetAlbumIndex, albums.size())) {
        if(mode & Playlist::RepeatPlaylist) {
            targetAlbumIndex = 0;
        }
        else {
            return -1;
        }
    }

    const auto& album = albums.at(targetAlbumIndex);
    return album.empty() ? -1 : album.front();
}

int PlaylistPrivate::getShuffleIndex(int delta, Playlist::PlayModes mode, bool onlyCheck)
{
    if(mode & Playlist::ShuffleTracks) {
        return handleTrackShuffle(delta, mode, onlyCheck);
    }
    if(mode & Playlist::ShuffleAlbums) {
        return handleAlbumShuffle(delta, mode, onlyCheck);
    }

    return -1;
}

int PlaylistPrivate::handleTrackShuffle(int delta, Playlist::PlayModes mode, bool onlyCheck)
{
    if(m_trackShuffleOrder.empty()) {
        createShuffleOrder();
        m_trackShuffleIndex = 0;
    }

    int nextIndex = m_trackShuffleIndex + delta;

    if(mode & Playlist::RepeatPlaylist) {
        if(std::cmp_greater_equal(nextIndex, m_trackShuffleOrder.size())) {
            nextIndex = 0;
        }
        else if(nextIndex < 0) {
            nextIndex = static_cast<int>(m_trackShuffleOrder.size() - 1);
        }
    }

    if(!onlyCheck) {
        m_trackShuffleIndex = nextIndex;
    }

    if(nextIndex >= 0 && std::cmp_less(nextIndex, m_trackShuffleOrder.size())) {
        return m_trackShuffleOrder.at(nextIndex);
    }

    return -1;
}

int PlaylistPrivate::handleAlbumShuffle(int delta, Playlist::PlayModes mode, bool onlyCheck)
{
    if(m_albumShuffleOrder.empty()) {
        createAlbumShuffleOrder();
        m_albumShuffleIndex = 0;
    }
    else if(m_albumShuffleIndex < 0) {
        m_albumShuffleIndex = 0;
    }

    if(m_albumShuffleIndex < 0 || std::cmp_greater_equal(m_albumShuffleIndex, m_albumShuffleOrder.size())) {
        return -1;
    }

    AlbumTracks currentAlbum = m_albumShuffleOrder.at(m_albumShuffleIndex);

    int nextIndex         = m_trackInAlbumIndex + delta;
    int albumShuffleIndex = m_albumShuffleIndex;

    if(std::cmp_greater_equal(nextIndex, currentAlbum.size())) {
        albumShuffleIndex = handleNextAlbum(mode, albumShuffleIndex, currentAlbum, nextIndex);
    }
    else if(nextIndex < 0) {
        albumShuffleIndex = handlePreviousAlbum(mode, albumShuffleIndex, currentAlbum, nextIndex);
    }

    if(!onlyCheck) {
        m_albumShuffleIndex = albumShuffleIndex;
        m_trackInAlbumIndex = nextIndex;
    }

    if(nextIndex < 0 || std::cmp_greater_equal(nextIndex, currentAlbum.size())) {
        return -1;
    }

    return currentAlbum.at(nextIndex);
}

int PlaylistPrivate::handleNextAlbum(Playlist::PlayModes mode, int albumShuffleIndex, AlbumTracks& currentAlbum,
                                     int& nextIndex)
{
    if(mode & Playlist::RepeatAlbum) {
        nextIndex = 0;
        return albumShuffleIndex;
    }

    albumShuffleIndex++;
    if(std::cmp_greater_equal(albumShuffleIndex, m_albumShuffleOrder.size())) {
        if(mode & Playlist::RepeatPlaylist) {
            // Loop to the first album
            albumShuffleIndex = 0;
        }
        else {
            nextIndex = -1;
            return -1;
        }
    }
    nextIndex    = 0;
    currentAlbum = m_albumShuffleOrder.at(albumShuffleIndex);

    return albumShuffleIndex;
}

int PlaylistPrivate::handlePreviousAlbum(Playlist::PlayModes mode, int albumShuffleIndex, AlbumTracks& currentAlbum,
                                         int& nextIndex)
{
    if(mode & Playlist::RepeatAlbum) {
        nextIndex = static_cast<int>(currentAlbum.size()) - 1;
        return albumShuffleIndex;
    }

    if(albumShuffleIndex == 0) {
        if(mode & Playlist::RepeatPlaylist) {
            // Loop to the last album
            albumShuffleIndex = static_cast<int>(m_albumShuffleOrder.size()) - 1;
        }
        else {
            nextIndex = -1;
            return -1;
        }
    }
    else {
        --albumShuffleIndex;
    }
    currentAlbum = m_albumShuffleOrder.at(albumShuffleIndex);
    nextIndex    = static_cast<int>(currentAlbum.size()) - 1;

    return albumShuffleIndex;
}

int PlaylistPrivate::getNextIndex(int delta, Playlist::PlayModes mode, bool onlyCheck)
{
    if(m_tracks.empty()) {
        return -1;
    }

    if(mode & Playlist::RepeatTrack) {
        return m_currentTrackIndex;
    }

    const auto getRandomIndexInAlbum = [this]() {
        const AlbumTracks albumTracks = getAlbumTracks(m_currentTrackIndex);
        if(!albumTracks.empty()) {
            std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<int> dist(0, static_cast<int>(albumTracks.size()) - 1);
            return albumTracks.at(dist(gen));
        }
        return -1;
    };

    if(mode & Playlist::ShuffleTracks) {
        if(mode & Playlist::RepeatAlbum) {
            return getRandomIndexInAlbum();
        }
        return getShuffleIndex(delta, mode, onlyCheck);
    }

    if(mode & Playlist::ShuffleAlbums) {
        return getShuffleIndex(delta, mode, onlyCheck);
    }

    const int count = static_cast<int>(m_tracks.size());

    if(mode & Playlist::Random) {
        if(mode & Playlist::RepeatAlbum) {
            return getRandomIndexInAlbum();
        }

        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, count - 1);
        return dist(gen);
    }

    int nextIndex = m_currentTrackIndex;

    if(mode == Playlist::Default) {
        nextIndex += delta;
        if(nextIndex < 0 || nextIndex >= count) {
            nextIndex = -1;
        }
    }
    else if(mode & Playlist::RepeatPlaylist) {
        nextIndex += delta;
        if(nextIndex < 0) {
            nextIndex = count - 1;
        }
        else if(nextIndex >= count) {
            nextIndex = 0;
        }
    }
    else if(mode & Playlist::RepeatAlbum) {
        const AlbumTracks albumTracks = getAlbumTracks(m_currentTrackIndex);
        if(albumTracks.empty()) {
            return -1;
        }

        nextIndex += delta;

        const int startIndex = albumTracks.front();
        const int endIndex   = albumTracks.back();

        if(nextIndex < startIndex) {
            nextIndex = endIndex; // Loop back to the last track of the album
        }
        else if(nextIndex > endIndex) {
            nextIndex = startIndex; // Loop back to the first track of the album
        }
    }

    return nextIndex;
}

int PlaylistPrivate::getNextIndexFrom(const int currentIndex, const int delta, const Playlist::PlayModes mode,
                                      const bool onlyCheck)
{
    NavigationState prevState = navigationState();

    const bool canKeepTrackShufflePreview
        = onlyCheck && (mode & Playlist::ShuffleTracks) && prevState.trackShuffleOrder.empty();
    const bool canKeepAlbumShufflePreview
        = onlyCheck && (mode & Playlist::ShuffleAlbums) && prevState.albumShuffleOrder.empty();

    m_currentTrackIndex = currentIndex;
    syncShuffleStateToCurrentTrack();

    const int nextIndex          = getNextIndex(delta, mode, onlyCheck);
    NavigationState previewState = navigationState();

    restoreNavigationState(std::move(prevState));

    if(canKeepTrackShufflePreview && !previewState.trackShuffleOrder.empty()) {
        m_trackShuffleIndex = previewState.trackShuffleIndex;
        m_trackShuffleOrder = std::move(previewState.trackShuffleOrder);
    }
    else if(canKeepAlbumShufflePreview && !previewState.albumShuffleOrder.empty()) {
        m_albumShuffleIndex = previewState.albumShuffleIndex;
        m_trackInAlbumIndex = previewState.trackInAlbumIndex;
        m_albumShuffleOrder = std::move(previewState.albumShuffleOrder);
    }

    return nextIndex;
}

std::optional<Track> PlaylistPrivate::getTrack(int index) const
{
    if(m_tracks.empty() || index < 0 || std::cmp_greater_equal(index, m_tracks.size())) {
        return {};
    }

    return m_tracks.at(index);
}

std::optional<PlaylistTrack> PlaylistPrivate::getPlaylistTrack(int index) const
{
    if(const auto track = getTrack(index)) {
        const UId entryId = std::cmp_less(index, m_trackEntryIds.size()) ? m_trackEntryIds.at(index) : UId{};
        return PlaylistTrack{.track = track.value(), .playlistId = m_id, .entryId = entryId, .indexInPlaylist = index};
    }

    return {};
}

int PlaylistPrivate::indexOfTrackEntry(const UId& entryId) const
{
    if(!entryId.isValid()) {
        return -1;
    }

    const auto it = std::ranges::find(m_trackEntryIds, entryId);
    if(it == m_trackEntryIds.cend()) {
        return -1;
    }

    return static_cast<int>(std::distance(m_trackEntryIds.cbegin(), it));
}

void PlaylistPrivate::replacePlaylistTracks(const PlaylistTrackList& tracks)
{
    m_tracks.clear();
    m_trackEntryIds.clear();
    m_tracks.reserve(tracks.size());
    m_trackEntryIds.reserve(tracks.size());

    for(const PlaylistTrack& playlistTrack : tracks) {
        m_tracks.emplace_back(playlistTrack.track);
        m_trackEntryIds.emplace_back(playlistTrack.entryId.isValid() ? playlistTrack.entryId : UId::create());
    }
}

Playlist::Playlist(PrivateKey /*key*/, int dbId, QString name, int index, SettingsManager* settings)
    : p{std::make_unique<PlaylistPrivate>(dbId, std::move(name), index, settings)}
{ }

Playlist::~Playlist() = default;

UId Playlist::id() const
{
    return p->m_id;
}

int Playlist::dbId() const
{
    return p->m_dbId;
}

QString Playlist::name() const
{
    return p->m_name;
}

int Playlist::index() const
{
    return p->m_index;
}

const TrackList& Playlist::tracks() const
{
    return p->m_tracks;
}

PlaylistTrackList Playlist::playlistTracks() const
{
    PlaylistTrackList tracks;
    for(int i{0}; const auto& track : p->m_tracks) {
        const UId entryId = std::cmp_less(i, p->m_trackEntryIds.size()) ? p->m_trackEntryIds.at(i) : UId{};
        tracks.emplace_back(track, p->m_id, entryId, i++);
    }
    return tracks;
}

std::optional<Track> Playlist::track(int index) const
{
    return p->getTrack(index);
}

std::optional<PlaylistTrack> Playlist::playlistTrack(int index) const
{
    return p->getPlaylistTrack(index);
}

std::optional<PlaylistTrack> Playlist::playlistTrack(const UId& entryId) const
{
    return playlistTrack(indexOfTrackEntry(entryId));
}

int Playlist::trackCount() const
{
    return static_cast<int>(p->m_tracks.size());
}

int Playlist::indexOfTrackEntry(const UId& entryId) const
{
    return p->indexOfTrackEntry(entryId);
}

int Playlist::currentTrackIndex() const
{
    return p->m_currentTrackIndex;
}

Track Playlist::currentTrack() const
{
    if(p->m_currentTrackIndex >= 0 && p->m_currentTrackIndex < trackCount()) {
        return p->m_tracks.at(p->m_currentTrackIndex);
    }

    return {};
}

bool Playlist::modified() const
{
    return p->m_modified;
}

bool Playlist::tracksModified() const
{
    return p->m_tracksModified;
}

bool Playlist::isTemporary() const
{
    return p->m_isTemporary;
}

bool Playlist::isAutoPlaylist() const
{
    return p->m_isAutoPlaylist;
}

QString Playlist::query() const
{
    return p->m_query;
}

QString Playlist::sortQuery() const
{
    return p->m_sortQuery;
}

bool Playlist::forceSorted() const
{
    return p->m_forceSorted;
}

TrackList Playlist::autoPlaylistTracks(const TrackList& tracks) const
{
    if(!isAutoPlaylist()) {
        return p->m_tracks;
    }

    return p->updatedAutoTracks(tracks);
}

bool Playlist::regenerateTracks(const TrackList& tracks)
{
    if(!isAutoPlaylist()) {
        return false;
    }

    const TrackList filteredTracks = autoPlaylistTracks(tracks);

    if(filteredTracks != p->m_tracks) {
        replaceTracks(filteredTracks);
        return true;
    }

    return false;
}

int Playlist::nextIndex(int delta, PlayModes mode)
{
    return p->getNextIndex(delta, mode, true);
}

int Playlist::nextIndexFrom(int currentIndex, int delta, PlayModes mode)
{
    return p->getNextIndexFrom(currentIndex, delta, mode, true);
}

int Playlist::firstIndex(PlayModes mode)
{
    if(trackCount() <= 0) {
        return -1;
    }

    if(mode & ShuffleTracks) {
        if(p->m_trackShuffleOrder.empty()) {
            p->createShuffleOrder(false);
        }

        if(p->m_trackShuffleOrder.empty()) {
            return -1;
        }

        p->m_trackShuffleIndex = 0;
        return p->m_trackShuffleOrder.front();
    }

    if(mode & ShuffleAlbums) {
        if(p->m_albumShuffleOrder.empty()) {
            p->createAlbumShuffleOrder(false);
        }

        if(p->m_albumShuffleOrder.empty() || p->m_albumShuffleOrder.front().empty()) {
            return -1;
        }

        p->m_albumShuffleIndex = 0;
        p->m_trackInAlbumIndex = 0;
        return p->m_albumShuffleOrder.front().front();
    }

    if(mode & Random) {
        return p->getNextIndex(1, mode, true);
    }

    return 0;
}

int Playlist::lastIndex(PlayModes mode)
{
    if(trackCount() <= 0) {
        return -1;
    }

    if(mode & ShuffleTracks) {
        if(p->m_trackShuffleOrder.empty()) {
            p->createShuffleOrder(false);
        }

        if(p->m_trackShuffleOrder.empty()) {
            return -1;
        }

        p->m_trackShuffleIndex = static_cast<int>(p->m_trackShuffleOrder.size()) - 1;
        return p->m_trackShuffleOrder.back();
    }

    if(mode & ShuffleAlbums) {
        if(p->m_albumShuffleOrder.empty()) {
            p->createAlbumShuffleOrder(false);
        }

        if(p->m_albumShuffleOrder.empty() || p->m_albumShuffleOrder.back().empty()) {
            return -1;
        }

        p->m_albumShuffleIndex = static_cast<int>(p->m_albumShuffleOrder.size()) - 1;
        p->m_trackInAlbumIndex = static_cast<int>(p->m_albumShuffleOrder.back().size()) - 1;
        return p->m_albumShuffleOrder.back().back();
    }

    if(mode & Random) {
        return p->getNextIndex(-1, mode, true);
    }

    return trackCount() - 1;
}

int Playlist::randomAlbumIndexFrom(const int currentIndex)
{
    return p->randomAlbumIndexFrom(currentIndex);
}

int Playlist::randomTrackIndexFrom(const int currentIndex)
{
    return p->randomTrackIndexFrom(currentIndex);
}

int Playlist::previousAlbumIndexFrom(const int currentIndex, const PlayModes mode)
{
    return p->adjacentAlbumIndexFrom(currentIndex, -1, mode);
}

int Playlist::nextAlbumIndexFrom(const int currentIndex, const PlayModes mode)
{
    return p->adjacentAlbumIndexFrom(currentIndex, 1, mode);
}

Track Playlist::nextTrack(int delta, PlayModes mode)
{
    const int index = p->getNextIndex(delta, mode, true);

    if(index < 0 || index >= trackCount()) {
        return {};
    }

    return p->m_tracks.at(index);
}

Track Playlist::nextTrackFrom(int currentIndex, int delta, PlayModes mode)
{
    const int index = p->getNextIndexFrom(currentIndex, delta, mode, true);

    if(index < 0 || index >= trackCount()) {
        return {};
    }

    return p->m_tracks.at(index);
}

Track Playlist::nextTrackChangeFrom(int currentIndex, int delta, PlayModes mode)
{
    p->m_currentTrackIndex = currentIndex;
    p->syncShuffleStateToCurrentTrack();

    const int index = p->getNextIndex(delta, mode, false);

    if(index < 0 || index >= trackCount()) {
        changeCurrentIndex(-1);
        return {};
    }

    changeCurrentIndex(index);

    return currentTrack();
}

Track Playlist::nextTrackChange(int delta, PlayModes mode)
{
    const int index = p->getNextIndex(delta, mode, false);

    if(index < 0 || index >= trackCount()) {
        changeCurrentIndex(-1);
        return {};
    }

    changeCurrentIndex(index);

    return currentTrack();
}

void Playlist::changeCurrentIndex(int index)
{
    p->m_currentTrackIndex = index;
    p->syncShuffleStateToCurrentTrack();
}

void Playlist::reset()
{
    p->m_trackShuffleOrder.clear();
    p->m_trackShuffleIndex = -1;

    p->m_albumGroupOrder.clear();
    p->m_albumShuffleOrder.clear();
    p->m_albumShuffleIndex = -1;
    p->m_trackInAlbumIndex = -1;
}

void Playlist::resetFlags()
{
    p->m_modified       = false;
    p->m_tracksModified = false;
}

QStringList Playlist::supportedPlaylistExtensions()
{
    static const QStringList supportedExtensions = {u"cue"_s, u"m3u"_s, u"m3u8"_s, u"pls"_s};
    return supportedExtensions;
}

std::unique_ptr<Playlist> Playlist::create(const QString& name, SettingsManager* settings)
{
    auto playlist              = std::make_unique<Playlist>(PrivateKey{}, -1, name, -1, settings);
    playlist->p->m_isTemporary = true;
    return playlist;
}

std::unique_ptr<Playlist> Playlist::create(int dbId, const QString& name, int index, SettingsManager* settings)
{
    return std::make_unique<Playlist>(PrivateKey{}, dbId, name, index, settings);
}

std::unique_ptr<Playlist> Playlist::createAuto(int dbId, const QString& name, int index, const QString& query,
                                               const QString& sortQuery, bool forceSorted, SettingsManager* settings)
{
    auto playlist                 = std::make_unique<Playlist>(PrivateKey{}, dbId, name, index, settings);
    playlist->p->m_isAutoPlaylist = true;
    playlist->setQuery(query);
    playlist->setSortQuery(sortQuery);
    playlist->setForceSorted(forceSorted);
    return playlist;
}

void Playlist::setName(const QString& name)
{
    if(std::exchange(p->m_name, name) != name) {
        p->m_modified = true;
    }
}

void Playlist::setIndex(int index)
{
    if(std::exchange(p->m_index, index) != index) {
        p->m_modified = true;
    }
}

void Playlist::setQuery(const QString& query)
{
    if(std::exchange(p->m_query, query) != query) {
        p->m_modified = true;
    }
}

void Playlist::setSortQuery(const QString& query)
{
    if(std::exchange(p->m_sortQuery, query) != query) {
        p->m_modified = true;
    }
}

void Playlist::setForceSorted(bool forceSorted)
{
    if(std::exchange(p->m_forceSorted, forceSorted) != forceSorted) {
        p->m_modified = true;
    }
}

void Playlist::setModified(bool modified)
{
    p->m_modified = modified;
}

void Playlist::setTracksModified(bool modified)
{
    p->m_tracksModified = modified;
}

void Playlist::replaceTracks(const TrackList& tracks)
{
    replaceTracks(PlaylistTrack::fromTracks(tracks, p->m_id));
}

void Playlist::replaceTracks(const PlaylistTrackList& tracks)
{
    const PlaylistTrackList updatedTracks = PlaylistTrack::updateIndexes(tracks);
    const PlaylistTrackList currentTracks{playlistTracks()};

    const bool tracksChanged = currentTracks.size() != updatedTracks.size()
                            || !std::ranges::equal(currentTracks, updatedTracks,
                                                   [](const PlaylistTrack& current, const PlaylistTrack& updated) {
                                                       return current.playlistId == updated.playlistId
                                                           && current.entryId == updated.entryId
                                                           && current.indexInPlaylist == updated.indexInPlaylist
                                                           && current.track.sameDataAs(updated.track);
                                                   });

    if(tracksChanged) {
        const int previousCurrentIndex = currentTrackIndex();
        const UId previousCurrentEntry
            = playlistTrack(previousCurrentIndex).has_value() ? playlistTrack(previousCurrentIndex)->entryId : UId{};

        p->replacePlaylistTracks(updatedTracks);
        p->m_tracksModified = true;
        p->m_trackShuffleOrder.clear();
        p->m_albumGroupOrder.clear();
        p->m_albumShuffleOrder.clear();

        if(previousCurrentEntry.isValid()) {
            changeCurrentIndex(indexOfTrackEntry(previousCurrentEntry));
        }
        else if(trackCount() == 0 || previousCurrentIndex < 0) {
            changeCurrentIndex(-1);
        }
        else {
            changeCurrentIndex(std::min(previousCurrentIndex, trackCount() - 1));
        }
    }
}

void Playlist::appendTracks(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    const PlaylistTrackList playlistTracks = PlaylistTrack::fromTracks(tracks, p->m_id);
    p->m_tracks.reserve(p->m_tracks.size() + playlistTracks.size());
    p->m_trackEntryIds.reserve(p->m_trackEntryIds.size() + playlistTracks.size());

    for(const PlaylistTrack& playlistTrack : playlistTracks) {
        p->m_tracks.emplace_back(playlistTrack.track);
        p->m_trackEntryIds.emplace_back(playlistTrack.entryId);
    }

    p->m_tracksModified = true;
    p->m_trackShuffleOrder.clear();
    p->m_albumGroupOrder.clear();
    p->m_albumShuffleOrder.clear();
}

void Playlist::updateTrackAtIndex(int index, const Track& track)
{
    if(index < 0 || std::cmp_greater_equal(index, p->m_tracks.size())) {
        return;
    }

    if(p->m_tracks.at(index).sameIdentityAs(track)) {
        p->m_tracks[index] = track;
    }
}

std::vector<int> Playlist::removeTracks(const std::vector<int>& indexes)
{
    if(indexes.empty()) {
        return {};
    }

    std::vector<int> removedIndexes;
    std::set<int> indexesToRemove{indexes.cbegin(), indexes.cend()};

    auto updateAlbumShuffleOrder = [this](int removedIndex) {
        int i{0};
        while(std::cmp_less(i, p->m_albumShuffleOrder.size())) {
            auto& album = p->m_albumShuffleOrder.at(i);

            std::erase_if(album, [removedIndex](int trackIndex) { return trackIndex == removedIndex; });

            for(auto& trackIndex : album) {
                if(trackIndex > removedIndex) {
                    --trackIndex;
                }
            }

            if(album.empty()) {
                p->m_albumShuffleOrder.erase(p->m_albumShuffleOrder.begin() + i);
                p->m_albumShuffleIndex
                    = std::clamp(p->m_albumShuffleIndex - 1, 0, static_cast<int>(p->m_albumShuffleOrder.size()) - 1);
            }
            else {
                ++i;
            }
        }
    };

    if(!p->m_trackShuffleOrder.empty()) {
        const int shuffleIndex = p->m_trackShuffleIndex;
        for(int i{0}; i <= shuffleIndex; ++i) {
            if(std::cmp_less(i, p->m_trackShuffleOrder.size())) {
                if(indexesToRemove.contains(p->m_trackShuffleOrder.at(i))) {
                    --p->m_trackShuffleIndex;
                }
            }
        }
        p->m_trackShuffleIndex = std::max(p->m_trackShuffleIndex, 0);
    }

    int adjustedTrackIndex = currentTrackIndex();

    for(const int index : indexesToRemove | std::views::reverse) {
        if(index < 0 || std::cmp_greater_equal(index, p->m_tracks.size())) {
            continue;
        }

        if(index <= currentTrackIndex()) {
            adjustedTrackIndex = std::max(adjustedTrackIndex - 1, 0);
        }

        p->m_tracks.erase(p->m_tracks.begin() + index);
        if(std::cmp_less(index, p->m_trackEntryIds.size())) {
            p->m_trackEntryIds.erase(p->m_trackEntryIds.begin() + index);
        }
        removedIndexes.emplace_back(index);

        std::erase_if(p->m_trackShuffleOrder, [index](int num) { return num == index; });
        for(auto& num : p->m_trackShuffleOrder) {
            if(num > index) {
                --num;
            }
        }

        updateAlbumShuffleOrder(index);
    }

    std::erase_if(p->m_trackShuffleOrder, [](int num) { return num < 0; });

    changeCurrentIndex(adjustedTrackIndex);
    p->m_tracksModified = true;
    p->m_albumGroupOrder.clear();

    return removedIndexes;
}
} // namespace Fooyin
