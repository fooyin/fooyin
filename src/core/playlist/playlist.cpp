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

#include <core/track.h>
#include <utils/crypto.h>

#include <random>
#include <ranges>
#include <set>

namespace Fooyin {
struct Playlist::Private
{
    int id{-1};
    QString name;
    int index{-1};
    TrackList tracks;

    int currentTrackIndex{0};
    int nextTrackIndex{-1};

    int shuffleIndex{-1};
    std::vector<int> shuffleOrder;

    bool isVisible{true};
    bool modified{false};
    bool tracksModified{false};

    Private(int id_, QString name_, int index_)
        : id{id_}
        , name{std::move(name_)}
        , index{index_}
    { }

    void createShuffleOrder()
    {
        shuffleOrder.resize(tracks.size());
        std::iota(shuffleOrder.begin(), shuffleOrder.end(), 0);
        std::ranges::shuffle(shuffleOrder, std::mt19937{std::random_device{}()});

        // Move current track to start
        auto it = std::ranges::find(shuffleOrder, currentTrackIndex);
        if(it != shuffleOrder.end()) {
            std::rotate(shuffleOrder.begin(), it, it + 1);
        }
    }

    int getRandomIndex(PlayModes mode)
    {
        if(shuffleOrder.empty()) {
            createShuffleOrder();

            shuffleIndex = (mode & RepeatTrack) ? 0 : 1;
        }

        else if(mode & RepeatPlaylist) {
            if(shuffleIndex > static_cast<int>(shuffleOrder.size() - 1)) {
                shuffleIndex = 0;
            }
            else if(shuffleIndex < 0) {
                shuffleIndex = static_cast<int>(shuffleOrder.size() - 1);
            }
        }

        if(shuffleIndex >= 0 && shuffleIndex < static_cast<int>(shuffleOrder.size())) {
            return shuffleOrder.at(shuffleIndex);
        }

        return -1;
    }
};

Playlist::Playlist()
    : Playlist{-1, QStringLiteral(""), -1}
{ }

Playlist::Playlist(int id, QString name, int index)
    : p{std::make_unique<Private>(id, name, index)}
{ }

bool Playlist::isValid() const
{
    return p->id >= 0 && p->index >= 0 && !p->name.isEmpty();
}

Playlist::~Playlist() = default;

int Playlist::id() const
{
    return p->id;
}

QString Playlist::name() const
{
    return p->name;
}

int Playlist::index() const
{
    return p->index;
}

TrackList Playlist::tracks() const
{
    return p->tracks;
}

Track Playlist::track(int index) const
{
    if(p->tracks.empty() || index < 0 || index >= trackCount()) {
        return {};
    }

    return p->tracks.at(index);
}

int Playlist::trackCount() const
{
    return static_cast<int>(p->tracks.size());
}

int Playlist::currentTrackIndex() const
{
    return p->currentTrackIndex;
}

Track Playlist::currentTrack() const
{
    if(p->nextTrackIndex >= 0 && p->nextTrackIndex < trackCount()) {
        return p->tracks.at(p->nextTrackIndex);
    }

    if(p->currentTrackIndex >= 0 && p->currentTrackIndex < trackCount()) {
        return p->tracks.at(p->currentTrackIndex);
    }

    return {};
}

bool Playlist::modified() const
{
    return p->modified;
}

bool Playlist::tracksModified() const
{
    return p->tracksModified;
}

bool Playlist::isVisible() const
{
    return p->isVisible;
}

void Playlist::scheduleNextIndex(int index)
{
    if(index >= 0 && index < trackCount()) {
        p->nextTrackIndex = index;
    }
}

Track Playlist::nextTrack(int delta, PlayModes mode)
{
    int index = currentTrackIndex();

    if(p->nextTrackIndex >= 0) {
        index = p->nextTrackIndex;
    }
    else {
        const int count = trackCount();

        if(mode & ShuffleTracks) {
            if(!(mode & RepeatTrack)) {
                p->shuffleIndex += delta;
            }
            index = p->getRandomIndex(mode);
        }
        else if(mode & RepeatPlaylist) {
            index += delta;
            if(index < 0) {
                index = count - 1;
            }
            else if(index >= count) {
                index = 0;
            }
        }
        else if(mode == Default) {
            index += delta;
            if(index < 0 || index >= count) {
                index = -1;
            }
        }

        if(index < 0) {
            return {};
        }
    }

    changeCurrentIndex(index);

    return currentTrack();
}

void Playlist::changeCurrentIndex(int index)
{
    p->currentTrackIndex = index;
}

void Playlist::reset()
{
    p->shuffleOrder.clear();
}

void Playlist::resetFlags()
{
    p->modified       = false;
    p->tracksModified = false;
}

void Playlist::setId(int id)
{
    p->id = id;
}

void Playlist::setName(const QString& name)
{
    if(std::exchange(p->name, name) != name) {
        p->modified = true;
    }
}

void Playlist::setIndex(int index)
{
    if(std::exchange(p->index, index) != index) {
        p->modified = true;
    }
}

void Playlist::setModified(bool modified)
{
    p->modified = modified;
}

void Playlist::setTracksModified(bool modified)
{
    p->tracksModified = modified;
}

void Playlist::replaceTracks(const TrackList& tracks)
{
    if(std::exchange(p->tracks, tracks) != tracks) {
        setTracksModified(true);
        p->shuffleOrder.clear();
        p->nextTrackIndex = -1;
    }
}

void Playlist::appendTracks(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    std::ranges::copy(tracks, std::back_inserter(p->tracks));
    setTracksModified(true);
    p->shuffleOrder.clear();
}

std::vector<int> Playlist::removeTracks(const std::vector<int>& indexes)
{
    std::vector<int> removedIndexes;

    std::set<int> indexesToRemove{indexes.cbegin(), indexes.cend()};

    auto prevHistory = p->shuffleOrder | std::views::take(p->shuffleIndex + 1);
    for(const int index : prevHistory) {
        if(indexesToRemove.contains(index)) {
            p->shuffleIndex -= 1;
        }
    }

    int adjustedTrackIndex = currentTrackIndex();

    for(const int index : indexesToRemove | std::views::reverse) {
        if(index <= currentTrackIndex()) {
            adjustedTrackIndex = std::max(adjustedTrackIndex - 1, 0);
        }

        if(index >= 0 && std::cmp_less(index, p->tracks.size())) {
            p->tracks.erase(p->tracks.begin() + index);
            removedIndexes.emplace_back(index);

            std::erase_if(p->shuffleOrder, [index](int num) { return num == index; });
            std::ranges::transform(p->shuffleOrder, p->shuffleOrder.begin(),
                                   [index](int num) { return num > index ? num - 1 : num; });
        }
    }

    std::erase_if(p->shuffleOrder, [](int num) { return num < 0; });

    changeCurrentIndex(adjustedTrackIndex);

    if(indexesToRemove.contains(p->nextTrackIndex)) {
        p->nextTrackIndex = -1;
    }

    setTracksModified(true);

    return removedIndexes;
}

void Playlist::clear()
{
    if(!p->tracks.empty()) {
        p->tracks.clear();
        setTracksModified(true);
        p->shuffleOrder.clear();
    }
}
} // namespace Fooyin
