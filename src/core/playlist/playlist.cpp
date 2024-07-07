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
struct Playlist::PrivateKey
{
    PrivateKey() { }
    PrivateKey(PrivateKey const&) = default;
};

struct Playlist::Private
{
    UId m_id;
    int m_dbId{-1};
    QString m_name;
    int m_index{-1};
    TrackList m_tracks;

    int m_currentTrackIndex{0};
    int m_nextTrackIndex{-1};

    int m_shuffleIndex{-1};
    std::vector<int> m_shuffleOrder;

    bool m_isTemporary{false};
    bool m_modified{false};
    bool m_tracksModified{false};

    explicit Private(QString name)
        : m_id{UId::create()}
        , m_name{std::move(name)}
        , m_isTemporary{true}
    { }

    Private(int dbId, QString name, int index)
        : m_id{UId::create()}
        , m_dbId{dbId}
        , m_name{std::move(name)}
        , m_index{index}
    { }

    void createShuffleOrder()
    {
        m_shuffleOrder.resize(m_tracks.size());
        std::iota(m_shuffleOrder.begin(), m_shuffleOrder.end(), 0);
        std::ranges::shuffle(m_shuffleOrder, std::mt19937{std::random_device{}()});

        // Move current track to start
        auto it = std::ranges::find(m_shuffleOrder, m_currentTrackIndex);
        if(it != m_shuffleOrder.end()) {
            std::rotate(m_shuffleOrder.begin(), it, it + 1);
        }
    }

    int getRandomIndex(PlayModes mode)
    {
        if(m_shuffleOrder.empty()) {
            createShuffleOrder();

            m_shuffleIndex = (mode & RepeatTrack) ? 0 : 1;
        }

        else if(mode & RepeatPlaylist) {
            if(m_shuffleIndex > static_cast<int>(m_shuffleOrder.size() - 1)) {
                m_shuffleIndex = 0;
            }
            else if(m_shuffleIndex < 0) {
                m_shuffleIndex = static_cast<int>(m_shuffleOrder.size() - 1);
            }
        }

        if(m_shuffleIndex >= 0 && m_shuffleIndex < static_cast<int>(m_shuffleOrder.size())) {
            return m_shuffleOrder.at(m_shuffleIndex);
        }

        return -1;
    }

    int getNextIndex(int delta, PlayModes mode)
    {
        if(m_tracks.empty()) {
            return -1;
        }

        int nextIndex = m_currentTrackIndex;

        if(m_nextTrackIndex >= 0) {
            nextIndex        = m_nextTrackIndex;
            m_nextTrackIndex = -1;
        }
        else {
            const int count = static_cast<int>(m_tracks.size());

            if(mode & ShuffleTracks) {
                if(!(mode & RepeatTrack)) {
                    m_shuffleIndex += delta;
                }
                nextIndex = getRandomIndex(mode);
            }
            else if(mode & RepeatPlaylist) {
                nextIndex += delta;
                if(nextIndex < 0) {
                    nextIndex = count - 1;
                }
                else if(nextIndex >= count) {
                    nextIndex = 0;
                }
            }
            else if(mode == Default) {
                nextIndex += delta;
                if(nextIndex < 0 || nextIndex >= count) {
                    nextIndex = -1;
                }
            }
        }

        return nextIndex;
    }
};

Playlist::Playlist(PrivateKey /*key*/, QString name)
    : p{std::make_unique<Private>(std::move(name))}
{ }

Playlist::Playlist(PrivateKey /*key*/, int dbId, QString name, int index)
    : p{std::make_unique<Private>(dbId, std::move(name), index)}
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

Track Playlist::track(int index) const
{
    if(p->m_tracks.empty() || index < 0 || index >= trackCount()) {
        return {};
    }

    return p->m_tracks.at(index);
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
    return p->getNextIndex(delta, mode);
}

Track Playlist::nextTrack(int delta, PlayModes mode)
{
    const int index = p->getNextIndex(delta, mode);

    if(index < 0) {
        return {};
    }

    return p->m_tracks.at(index);
}

Track Playlist::nextTrackChange(int delta, PlayModes mode)
{
    const int index = p->getNextIndex(delta, mode);

    if(index < 0) {
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
    p->m_shuffleOrder.clear();
}

void Playlist::resetFlags()
{
    p->m_modified       = false;
    p->m_tracksModified = false;
}

QStringList Playlist::supportedPlaylistExtensions()
{
    static const QStringList supportedExtensions
        = {QStringLiteral("*.cue"), QStringLiteral("*.m3u"), QStringLiteral("*.m3u8")};
    return supportedExtensions;
}

std::unique_ptr<Playlist> Playlist::create(const QString& name)
{
    return std::make_unique<Playlist>(PrivateKey{}, name);
}

std::unique_ptr<Playlist> Playlist::create(int dbId, const QString& name, int index)
{
    return std::make_unique<Playlist>(PrivateKey{}, dbId, name, index);
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
        p->m_shuffleOrder.clear();
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
    p->m_shuffleOrder.clear();
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
    std::vector<int> removedIndexes;

    std::set<int> indexesToRemove{indexes.cbegin(), indexes.cend()};

    auto prevHistory = p->m_shuffleOrder | std::views::take(p->m_shuffleIndex + 1);
    for(const int index : prevHistory) {
        if(indexesToRemove.contains(index)) {
            p->m_shuffleIndex -= 1;
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

            std::erase_if(p->m_shuffleOrder, [index](int num) { return num == index; });
            std::ranges::transform(p->m_shuffleOrder, p->m_shuffleOrder.begin(),
                                   [index](int num) { return num > index ? num - 1 : num; });
        }
    }

    std::erase_if(p->m_shuffleOrder, [](int num) { return num < 0; });

    changeCurrentIndex(adjustedTrackIndex);

    if(indexesToRemove.contains(p->m_nextTrackIndex)) {
        p->m_nextTrackIndex = -1;
    }

    p->m_tracksModified = true;

    return removedIndexes;
}

void Playlist::clear()
{
    if(!p->m_tracks.empty()) {
        p->m_tracks.clear();
        p->m_tracksModified = true;
        p->m_shuffleOrder.clear();
    }
}
} // namespace Fooyin
