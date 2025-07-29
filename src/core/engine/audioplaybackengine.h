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

class QFileSystemWatcher;

namespace Fooyin {
class SettingsManager;
class SignalThrottler;

class AudioPlaybackEngine : public AudioEngine
{
    Q_OBJECT

public:
    explicit AudioPlaybackEngine(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings,
                                 QObject* parent = nullptr);
    ~AudioPlaybackEngine() override;

    void loadTrack(const Track& track) override;
    void prepareNextTrack(const Track& track) override;

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
    void resetNextTrack();
    AudioFormat loadPreparedTrack();
    void resetWorkers(bool resetFade = true);
    void stopWorkers(bool full = false);
    void startBitrateTimer();

    void handleOutputState(AudioOutput::State outState);
    void reloadOutput();

    void currentFileChanged();

    bool checkOpenSource();
    void setupDuration();
    bool checkReadyToDecode();

    void readNextBuffer();
    void updatePosition();
    void updateBitrate();
    void onBufferProcessed(const AudioBuffer& buffer);
    void onRendererFinished();

    [[nodiscard]] bool trackIsValid() const;
    [[nodiscard]] bool trackCanPlay() const;
    [[nodiscard]] bool isFading() const;
    [[nodiscard]] int calculateFadeLength(int initialValue) const;

    std::shared_ptr<AudioLoader> m_audioLoader;
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
    bool m_ending;
    bool m_decoding;
    bool m_updatingTrack;
    bool m_pauseNextTrack;
    std::optional<PlaybackState> m_pendingState;

    AudioDecoder* m_decoder;
    AudioDecoder* m_nextDecoder;
    AudioFormat m_format;
    AudioFormat m_nextFormat;

    Track m_currentTrack;
    Track m_nextTrack;
    AudioSource m_source;
    AudioSource m_nextSource;
    std::unique_ptr<QFile> m_file;
    std::unique_ptr<QFile> m_nextFile;

    QThread* m_outputThread;
    AudioRenderer m_renderer;
    QMetaObject::Connection m_pausedConnection;

    QBasicTimer m_posTimer;
    QBasicTimer m_bitrateTimer;
    QBasicTimer m_bufferTimer;
    QBasicTimer m_pauseTimer;

    FadingIntervals m_fadeIntervals;
    std::optional<uint64_t> m_pendingSeek;

    QFileSystemWatcher* m_trackWatcher;
    SignalThrottler* m_watcherThrottler;
};
} // namespace Fooyin
