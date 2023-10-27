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

#pragma once

#include "fycore_export.h"

#include "core/track.h"

#include <QObject>

namespace Fy::Core {

namespace Player {
enum class PlayMode;
}

namespace Playlist {
class FYCORE_EXPORT Playlist
{
public:
    Playlist();
    explicit Playlist(QString name, int index, int id);

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] int id() const;
    [[nodiscard]] int index() const;
    [[nodiscard]] QString name() const;

    [[nodiscard]] TrackList tracks() const;
    [[nodiscard]] int trackCount() const;

    [[nodiscard]] int currentTrackIndex() const;
    [[nodiscard]] Track currentTrack() const;
    [[nodiscard]] bool modified() const;
    [[nodiscard]] bool tracksModified() const;

    void scheduleNextIndex(int index);
    Track nextTrack(Player::PlayMode mode, int delta);

    void setIndex(int index);
    void setName(const QString& name);

    void replaceTracks(const TrackList& tracks);
    void replaceTracksSilently(const TrackList& tracks);
    void appendTracks(const TrackList& tracks);
    void appendTracksSilently(const TrackList& tracks);

    void clear();
    void resetFlags();

    void changeCurrentTrack(int index);
    void changeCurrentTrack(const Core::Track& track);

private:
    int m_id;
    int m_index;
    QString m_name;

    TrackList m_tracks;
    int m_currentTrackIndex;
    int m_nextTrackIndex;

    bool m_modified;
    bool m_tracksModified;
};
using PlaylistList = std::vector<std::unique_ptr<Playlist>>;
} // namespace Playlist
} // namespace Fy::Core
