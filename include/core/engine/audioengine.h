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

namespace Fooyin {
class Track;

enum class PlaybackState
{
    Stopped,
    Playing,
    Fading,
    Paused,
    Error
};

enum class TrackStatus
{
    NoTrack,
    Loading,
    Loaded,
    Buffered,
    End,
    Invalid,
    Unreadable
};

class FYCORE_EXPORT AudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit AudioEngine(QObject* parent = nullptr)
        : QObject{parent}
    { }

    virtual void seek(uint64_t pos) = 0;

    virtual void changeTrack(const Track& track) = 0;

    virtual void play()  = 0;
    virtual void pause() = 0;
    virtual void stop()  = 0;

    virtual void setVolume(double volume) = 0;

    virtual void setAudioOutput(const OutputCreator& output, const QString& device) = 0;
    virtual void setOutputDevice(const QString& device)                             = 0;

signals:
    void deviceError(const QString& error);
    void stateChanged(PlaybackState state);
    void trackStatusChanged(TrackStatus status);
    void positionChanged(uint64_t ms);
    void trackChanged(const Fooyin::Track& track);
    void trackAboutToFinish();
    void finished();
};
} // namespace Fooyin
