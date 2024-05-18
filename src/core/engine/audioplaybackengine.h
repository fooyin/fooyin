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

#pragma once

#include <core/engine/audioengine.h>

namespace Fooyin {
class SettingsManager;

class AudioPlaybackEngine : public AudioEngine
{
    Q_OBJECT

public:
    explicit AudioPlaybackEngine(SettingsManager* settings, QObject* parent = nullptr);
    ~AudioPlaybackEngine() override;

public slots:
    void seek(uint64_t pos) override;

    void changeTrack(const Track& track) override;
    void setState(PlaybackState state) override;

    void play() override;
    void pause() override;
    void stop() override;

    void setVolume(double volume) override;

    void setAudioOutput(const OutputCreator& output) override;
    void setOutputDevice(const QString& device) override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
