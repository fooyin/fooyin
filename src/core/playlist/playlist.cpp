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

#include <core/playlist/playlist.h>

#include <core/coresettings.h>
#include <core/library/tracksort.h>
#include <core/scripting/scriptparser.h>
#include <core/track.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>

#include <random>
#include <ranges>
#include <set>

namespace {
using AlbumTracks = std::vector<int>;

struct TrackIndex
{
    Fooyin::Track track;
    int index;

    static Fooyin::Track& extractor(TrackIndex& item)
    {
        return item.track;
    };

    static const Fooyin::Track& extractorConst(const TrackIndex& item)
    {
        return item.track;
    };
};
} // namespace

namespace Fooyin {
struct Playlist::PrivateKey
{
    PrivateKey() { }
    PrivateKey(PrivateKey const&) = default;
};

class PlaylistPrivate
{
public:
    explicit PlaylistPrivate(QString name, SettingsManager* settings);
    PlaylistPrivate(int dbId, QString name, int index, SettingsManager* settings);

    void createShuffleOrder();
    void createAlbumShuffleOrder();

    AlbumTracks getAlbumTracks(int currentIndex);
    void sortAlbumTracks(AlbumTracks& album, const QString& sortScript);

    int getShuffleIndex(int delta, Playlist::PlayModes mode, bool onlyCheck);
    int handleTrackShuffle(int delta, Playlist::PlayModes mode, bool onlyCheck);
    int handleAlbumShuffle(int delta, Playlist::PlayModes mode, bool onlyCheck);
    int handleNextAlbum(Playlist::PlayModes mode, int albumShuffleIndex, AlbumTracks& currentAlbum, int& nextIndex);
    int handlePreviousAlbum(Playlist::PlayModes mode, int albumShuffleIndex, AlbumTracks& currentAlbum, int& nextIndex);

    int getNextIndex(int delta, Playlist::PlayModes mode, bool onlyCheck);
    [[nodiscard]] std::optional<Track> getTrack(int index) const;

    UId m_id;
    int m_dbId{-1};
    QString m_name;
    int m_index{-1};
    TrackList m_tracks;

    SettingsManager* m_settings;
    ScriptParser m_parser;
    TrackSorter m_sorter;

    int m_currentTrackIndex{0};
    int m_nextTrackIndex{-1};

    int m_trackShuffleIndex{-1};
    std::vector<int> m_trackShuffleOrder;

    int m_albumShuffleIndex{-1};
    int m_trackInAlbumIndex{-1};
    std::vector<AlbumTracks> m_albumShuffleOrder;

    bool m_isTemporary{false};
    bool m_modified{false};
    bool m_tracksModified{false};
};

PlaylistPrivate::PlaylistPrivate(QString name, SettingsManager* settings)
    : PlaylistPrivate{-1, std::move(name), -1, settings}
{
    m_isTemporary = true;
}

PlaylistPrivate::PlaylistPrivate(int dbId, QString name, int index, SettingsManager* settings)
    : m_id{UId::create()}
    , m_dbId{dbId}
    , m_name{std::move(name)}
    , m_index{index}
    , m_settings{settings}
{ }

void PlaylistPrivate::createShuffleOrder()
{
    m_trackShuffleOrder.resize(m_tracks.size());
    std::iota(m_trackShuffleOrder.begin(), m_trackShuffleOrder.end(), 0);
    std::ranges::shuffle(m_trackShuffleOrder, std::mt19937{std::random_device{}()});

    // Move current track to start
    auto it = std::ranges::find(m_trackShuffleOrder, m_currentTrackIndex);
    if(it != m_trackShuffleOrder.end()) {
        std::rotate(m_trackShuffleOrder.begin(), it, it + 1);
    }
}

void PlaylistPrivate::createAlbumShuffleOrder()
{
    m_albumShuffleOrder.clear();

    std::unordered_map<QString, AlbumTracks> albums;

    const QString groupScript = m_settings->value<Settings::Core::ShuffleAlbumsGroupScript>();

    const auto count = static_cast<int>(m_tracks.size());
    for(int i{0}; i < count; ++i) {
        const auto& track        = m_tracks.at(i);
        const QString albumGroup = m_parser.evaluate(groupScript, track);
        albums[albumGroup].emplace_back(i);
    }

    AlbumTracks currentAlbum;

    for(auto& [albumGroup, albumTracks] : albums) {
        if(std::ranges::find(albumTracks, m_currentTrackIndex) != albumTracks.end()) {
            currentAlbum = albumTracks;
        }

        const QString sortScript = m_settings->value<Settings::Core::ShuffleAlbumsSortScript>();
        if(!sortScript.isEmpty()) {
            sortAlbumTracks(albumTracks, sortScript);
        }

        m_albumShuffleOrder.push_back(albumTracks);
    }

    std::ranges::shuffle(m_albumShuffleOrder, std::mt19937{std::random_device{}()});

    // Move the current album to the front
    if(!currentAlbum.empty()) {
        auto it = std::ranges::find(m_albumShuffleOrder, currentAlbum);
        if(it != m_albumShuffleOrder.end()) {
            std::rotate(m_albumShuffleOrder.begin(), it, it + 1);
        }
    }

    m_albumShuffleIndex = 0;
    m_trackInAlbumIndex = 0;
}

AlbumTracks PlaylistPrivate::getAlbumTracks(int currentIndex)
{
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

void PlaylistPrivate::sortAlbumTracks(AlbumTracks& album, const QString& sortScript)
{
    std::vector<TrackIndex> trackIndexes;
    trackIndexes.reserve(album.size());

    for(const int trackIndex : album) {
        trackIndexes.emplace_back(m_tracks.at(trackIndex), trackIndex);
    }

    m_sorter.calcSortTracks(sortScript, trackIndexes, TrackIndex::extractor, TrackIndex::extractorConst);

    album.clear();
    for(const auto& track : trackIndexes) {
        album.emplace_back(track.index);
    }
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
        m_trackInAlbumIndex = 0;
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
    if(albumShuffleIndex >= static_cast<int>(m_albumShuffleOrder.size())) {
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

    if(m_nextTrackIndex >= 0) {
        return std::exchange(m_nextTrackIndex, -1);
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

std::optional<Track> PlaylistPrivate::getTrack(int index) const
{
    if(m_tracks.empty() || index < 0 || std::cmp_greater_equal(index, m_tracks.size())) {
        return {};
    }

    return m_tracks.at(index);
}

Playlist::Playlist(PrivateKey /*key*/, QString name, SettingsManager* settings)
    : p{std::make_unique<PlaylistPrivate>(std::move(name), settings)}
{ }

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

TrackList Playlist::tracks() const
{
    return p->m_tracks;
}

std::optional<Track> Playlist::track(int index) const
{
    return p->getTrack(index);
}

int Playlist::trackCount() const
{
    return static_cast<int>(p->m_tracks.size());
}

int Playlist::currentTrackIndex() const
{
    return p->m_currentTrackIndex;
}

Track Playlist::currentTrack() const
{
    if(p->m_nextTrackIndex >= 0 && p->m_nextTrackIndex < trackCount()) {
        return p->m_tracks.at(p->m_nextTrackIndex);
    }

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

void Playlist::scheduleNextIndex(int index)
{
    if(index >= 0 && index < trackCount()) {
        p->m_nextTrackIndex = index;
    }
}

int Playlist::nextIndex(int delta, PlayModes mode)
{
    return p->getNextIndex(delta, mode, true);
}

Track Playlist::nextTrack(int delta, PlayModes mode)
{
    const int index = p->getNextIndex(delta, mode, true);

    if(index < 0) {
        return {};
    }

    return p->m_tracks.at(index);
}

Track Playlist::nextTrackChange(int delta, PlayModes mode)
{
    const int index = p->getNextIndex(delta, mode, false);

    if(index < 0) {
        changeCurrentIndex(-1);
        return {};
    }

    changeCurrentIndex(index);

    return currentTrack();
}

void Playlist::changeCurrentIndex(int index)
{
    p->m_currentTrackIndex = index;
}

void Playlist::reset()
{
    p->m_trackShuffleOrder.clear();
    p->m_trackShuffleIndex = -1;

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
    static const QStringList supportedExtensions
        = {QStringLiteral("cue"), QStringLiteral("m3u"), QStringLiteral("m3u8")};
    return supportedExtensions;
}

std::unique_ptr<Playlist> Playlist::create(const QString& name, SettingsManager* settings)
{
    return std::make_unique<Playlist>(PrivateKey{}, name, settings);
}

std::unique_ptr<Playlist> Playlist::create(int dbId, const QString& name, int index, SettingsManager* settings)
{
    return std::make_unique<Playlist>(PrivateKey{}, dbId, name, index, settings);
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
    if(std::exchange(p->m_tracks, tracks) != tracks) {
        p->m_tracksModified = true;
        p->m_trackShuffleOrder.clear();
        p->m_albumShuffleOrder.clear();
        p->m_nextTrackIndex = -1;
    }
}

void Playlist::appendTracks(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    std::ranges::copy(tracks, std::back_inserter(p->m_tracks));
    p->m_tracksModified = true;
    p->m_trackShuffleOrder.clear();
    p->m_albumShuffleOrder.clear();
}

void Playlist::updateTrackAtIndex(int index, const Track& track)
{
    if(index < 0 || std::cmp_greater_equal(index, p->m_tracks.size())) {
        return;
    }

    if(p->m_tracks.at(index).uniqueFilepath() == track.uniqueFilepath()) {
        p->m_tracks[index] = track;
    }
}

std::vector<int> Playlist::removeTracks(const std::vector<int>& indexes)
{
    const auto updateAlbumShuffleOrder = [this](int removedIndex) {
        for(auto& album : p->m_albumShuffleOrder) {
            std::erase_if(album, [removedIndex](int trackIndex) { return trackIndex == removedIndex; });
            std::ranges::transform(album, album.begin(), [removedIndex](int trackIndex) {
                return trackIndex > removedIndex ? trackIndex - 1 : trackIndex;
            });
            if(album.empty()) {
                std::erase_if(p->m_albumShuffleOrder, [&album](const auto& a) { return a == album; });
            }
        }
    };

    std::vector<int> removedIndexes;
    std::set<int> indexesToRemove{indexes.cbegin(), indexes.cend()};

    auto prevHistory = p->m_trackShuffleOrder | std::views::take(p->m_trackShuffleIndex + 1);
    for(const int index : prevHistory) {
        if(indexesToRemove.contains(index)) {
            p->m_trackShuffleIndex -= 1;
        }
    }

    int adjustedTrackIndex = currentTrackIndex();

    for(const int index : indexesToRemove | std::views::reverse) {
        if(index <= currentTrackIndex()) {
            adjustedTrackIndex = std::max(adjustedTrackIndex - 1, 0);
        }

        if(index >= 0 && std::cmp_less(index, p->m_tracks.size())) {
            p->m_tracks.erase(p->m_tracks.begin() + index);
            removedIndexes.emplace_back(index);

            std::erase_if(p->m_trackShuffleOrder, [index](int num) { return num == index; });
            std::ranges::transform(p->m_trackShuffleOrder, p->m_trackShuffleOrder.begin(),
                                   [index](int num) { return num > index ? num - 1 : num; });

            updateAlbumShuffleOrder(index);
        }
    }

    std::erase_if(p->m_trackShuffleOrder, [](int num) { return num < 0; });
    changeCurrentIndex(adjustedTrackIndex);
    if(indexesToRemove.contains(p->m_nextTrackIndex)) {
        p->m_nextTrackIndex = -1;
    }

    p->m_tracksModified = true;

    return removedIndexes;
}
} // namespace Fooyin
