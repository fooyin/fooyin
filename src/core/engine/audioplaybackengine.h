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

#include "audioclock.h"
#include "internalcoresettings.h"

#include <core/engine/audioengine.h>
#include <core/engine/audioloader.h>
#include <core/track.h>

#include <QBasicTimer>

namespace Fooyin {
class AudioRenderer;
class SettingsManager;

class AudioPlaybackEngine : public AudioEngine
{
    Q_OBJECT

public:
    explicit AudioPlaybackEngine(std::shared_ptr<AudioLoader> decoderProvider, SettingsManager* settings,
                                 QObject* parent = nullptr);
    ~AudioPlaybackEngine() override;

    void seek(uint64_t pos) override;

    void changeTrack(const Fooyin::Track& track) override;

    void play() override;
    void pause() override;
    void stop() override;

    void setVolume(double volume) override;

    void setAudioOutput(const Fooyin::OutputCreator& output, const QString& device) override;
    void setOutputDevice(const QString& device) override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void resetWorkers();
    void stopWorkers(bool full = false);

    void handleOutputState(AudioOutput::State outState);
    void updateState(PlaybackState newState);
    TrackStatus changeTrackStatus(TrackStatus newStatus);

    void setupDuration();
    bool updateFormat(const AudioFormat& nextFormat);

    void startPlayback();
    void playOutput();
    void pauseOutput();
    void stopOutput();

    void readNextBuffer();
    void updatePosition();
    void onBufferProcessed(const AudioBuffer& buffer);
    void onRendererFinished();

    std::shared_ptr<AudioLoader> m_decoderProvider;
    AudioInput* m_decoder;
    SettingsManager* m_settings;

    AudioClock m_clock;

    TrackStatus m_status;
    PlaybackState m_state;
    AudioOutput::State m_outputState;

    uint64_t m_startPosition;
    uint64_t m_endPosition;
    uint64_t m_lastPosition;

    uint64_t m_totalBufferTime;
    uint64_t m_bufferLength;

    uint64_t m_duration;
    double m_volume;
    bool m_ending;
    bool m_decoding;

    Track m_currentTrack;
    AudioFormat m_format;

    AudioRenderer* m_renderer;

    QBasicTimer m_posTimer;
    QBasicTimer m_bufferTimer;
    QBasicTimer m_pauseTimer;

    FadingIntervals m_fadeIntervals;
};
} // namespace Fooyin
