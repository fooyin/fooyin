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

#include <core/playlist/playlist.h>
#include <core/trackfwd.h>

namespace Fooyin {
class PlaylistHandler;

struct PlaylistInfo
{
    int id;
    QString name;
    int index;
};

class FyPlaylist : public Playlist
{
public:
    explicit FyPlaylist(const PlaylistInfo& info);
    ~FyPlaylist() override;

    [[nodiscard]] bool isValid() const override;

    [[nodiscard]] int id() const override;
    [[nodiscard]] int index() const override;
    [[nodiscard]] QString name() const override;

    [[nodiscard]] TrackList tracks() const override;
    [[nodiscard]] std::optional<Track> track(int index) const override;
    [[nodiscard]] int trackCount() const override;

    [[nodiscard]] int currentTrackIndex() const override;
    [[nodiscard]] Track currentTrack() const override;

    [[nodiscard]] bool modified() const override;
    [[nodiscard]] bool tracksModified() const override;

    void scheduleNextIndex(int index) override;

    Track nextTrack(int delta, PlayModes mode) override;

    void setIndex(int index) override;
    void setName(const QString& name) override;

    void replaceTracks(const TrackList& tracks) override;
    void replaceTracksSilently(const TrackList& tracks) override;
    void appendTracks(const TrackList& tracks) override;
    void appendTracksSilently(const TrackList& tracks) override;
    void removeTracks(const IndexSet& indexes) override;

    void clear() override;
    void reset() override;
    void resetFlags() override;

    void changeCurrentTrack(int index) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
