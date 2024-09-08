/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/outputplugin.h>

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(ENGINE)

namespace Fooyin {
class Track;

class FYCORE_EXPORT AudioEngine : public QObject
{
    Q_OBJECT

public:
    enum class PlaybackState : uint8_t
    {
        Stopped = 0,
        Playing,
        FadingOut,
        Paused,
        Error
    };
    Q_ENUM(PlaybackState)

    enum class TrackStatus : uint8_t
    {
        NoTrack = 0,
        Loading,
        Loaded,
        Buffered,
        End,
        Invalid,
        Unreadable
    };
    Q_ENUM(TrackStatus)

    explicit AudioEngine(QObject* parent = nullptr)
        : QObject{parent}
    { }

    [[nodiscard]] PlaybackState playbackState() const;
    [[nodiscard]] TrackStatus trackStatus() const;

    virtual void changeTrack(const Track& track) = 0;

    virtual void play()  = 0;
    virtual void pause() = 0;
    virtual void stop()  = 0;

    virtual void seek(uint64_t pos)       = 0;
    virtual void setVolume(double volume) = 0;

    virtual void setAudioOutput(const OutputCreator& output, const QString& device) = 0;
    virtual void setOutputDevice(const QString& device)                             = 0;

signals:
    void deviceError(const QString& error);
    void stateChanged(PlaybackState state);
    void trackStatusChanged(TrackStatus status);
    void positionChanged(const Fooyin::Track& track, uint64_t ms);
    void trackChanged(const Fooyin::Track& track);
    void trackAboutToFinish();
    void finished();

protected:
    virtual PlaybackState updateState(PlaybackState state);
    virtual TrackStatus updateTrackStatus(TrackStatus status);

private:
    std::atomic<PlaybackState> m_playbackState;
    std::atomic<TrackStatus> m_trackStatus;
};
} // namespace Fooyin
