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

#include "core/engine/audioengine.h"

class AVFormatContext;

namespace Fy::Core::Engine::FFmpeg {
class Codec;

class FFmpegEngine : public AudioEngine
{
    Q_OBJECT

public:
    explicit FFmpegEngine(QObject* parent = nullptr);
    ~FFmpegEngine() override;

    void shutdown() override;

    void seek(uint64_t pos) override;
    [[nodiscard]] uint64_t currentPosition() const;

    void changeTrack(const Track& track) override;
    void setState(PlaybackState state);

    void play() override;
    void pause() override;
    void stop() override;

    void setVolume(double volume) override;

    void setAudioOutput(AudioOutput* output) override;
    void setOutputDevice(const QString& device) override;

signals:
    void resetWorkers();
    void killWorkers();

    void pauseOutput(bool pause);
    void updateOutput(AudioOutput* output);
    void updateDevice(const QString& output);

    void startDecoder(AVFormatContext* context, Codec* codec);
    void startRenderer(Codec* codec, AudioOutput* output);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fy::Core::Engine::FFmpeg
