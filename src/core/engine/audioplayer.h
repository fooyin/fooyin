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

#include <utils/worker.h>

#include <QObject>

namespace Fy::Core {
class Track;

namespace Engine {
enum PlaybackState
{
    StoppedState,
    PlayingState,
    PausedState
};

enum TrackStatus
{
    NoTrack,
    LoadingTrack,
    LoadedTrack,
    BufferedTrack,
    EndOfTrack,
    InvalidTrack
};

class AudioPlayer : public Utils::Worker
{
    Q_OBJECT

public:
    explicit AudioPlayer(QObject* parent = nullptr);
    ~AudioPlayer();

    void init();

    Track track() const;

    PlaybackState playbackState() const;
    TrackStatus trackStatus() const;

    uint64_t position() const;
    bool isPlaying() const;

    void play();
    void stop();
    void pause();

    void seek(uint64_t position);
    void setVolume(float value);

    void changeTrack(const Track& track);

signals:
    void trackChanged(const Track& track);
    void trackFinished();

    void playbackStateChanged(PlaybackState newState);
    void trackStatusChanged(TrackStatus status);

    void positionChanged(uint64_t ms);

private:
    friend class AudioEngine;

    void setState(PlaybackState ps);
    void setStatus(TrackStatus status);

    struct Private;
    std::unique_ptr<AudioPlayer::Private> p;
};
} // namespace Engine
} // namespace Fy::Core
