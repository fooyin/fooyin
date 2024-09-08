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
#include "audiorenderer.h"
#include "internalcoresettings.h"

#include <core/engine/audioengine.h>
#include <core/engine/audioloader.h>
#include <core/track.h>

#include <QBasicTimer>
#include <QFile>

namespace Fooyin {
class SettingsManager;

class AudioPlaybackEngine : public AudioEngine
{
    Q_OBJECT

public:
    explicit AudioPlaybackEngine(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings,
                                 QObject* parent = nullptr);
    ~AudioPlaybackEngine() override;

    void changeTrack(const Track& track) override;

    void play() override;
    void pause() override;
    void stop() override;

    void seek(uint64_t pos) override;
    void setVolume(double volume) override;

    void setAudioOutput(const OutputCreator& output, const QString& device) override;
    void setOutputDevice(const QString& device) override;

protected:
    void timerEvent(QTimerEvent* event) override;

    PlaybackState updateState(PlaybackState state) override;

private:
    void resetWorkers();
    void stopWorkers(bool full = false);

    void handleOutputState(AudioOutput::State outState);

    bool checkOpenSource();
    void setupDuration();
    void updateFormat(const AudioFormat& nextFormat, const std::function<void(bool)>& callback);
    bool checkReadyToDecode();
    bool waitForTrackLoaded(PlaybackState state);

    void readNextBuffer();
    void updatePosition();
    void onBufferProcessed(const AudioBuffer& buffer);
    void onRendererFinished();

    [[nodiscard]] bool trackIsValid() const;
    [[nodiscard]] bool trackCanPlay() const;
    [[nodiscard]] bool isFading() const;
    [[nodiscard]] int calculateFadeLength(int initialValue) const;

    std::shared_ptr<AudioLoader> m_audioLoader;
    AudioDecoder* m_decoder;
    SettingsManager* m_settings;

    AudioClock m_clock;

    AudioOutput::State m_outputState;

    uint64_t m_startPosition;
    uint64_t m_endPosition;
    uint64_t m_lastPosition;

    uint64_t m_totalBufferTime;
    uint64_t m_bufferLength;

    uint64_t m_duration;
    double m_volume;
    float m_trackPeak;
    bool m_ending;
    bool m_decoding;
    bool m_updatingTrack;
    bool m_pauseNextTrack;

    Track m_currentTrack;
    AudioSource m_source;
    std::unique_ptr<QFile> m_file;
    AudioFormat m_format;

    QThread* m_outputThread;
    AudioRenderer m_renderer;

    QBasicTimer m_posTimer;
    QBasicTimer m_bufferTimer;
    QBasicTimer m_pauseTimer;

    FadingIntervals m_fadeIntervals;
    std::optional<uint64_t> m_pendingSeek;
};
} // namespace Fooyin
