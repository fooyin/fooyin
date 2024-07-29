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

#include <core/engine/audiooutput.h>

#include "ffmpeg/ffmpegresampler.h"

#include <QBasicTimer>
#include <QObject>

#include <queue>

namespace Fooyin {
class AudioBuffer;
class AudioFormat;

class AudioRenderer : public QObject
{
    Q_OBJECT

public:
    explicit AudioRenderer(QObject* parent = nullptr);
    ~AudioRenderer() override;

    bool init(const AudioFormat& format);
    void start();
    void stop();
    void closeOutput();
    void reset();

    [[nodiscard]] bool isPaused() const;
    [[nodiscard]] bool isFading() const;
    void pause(bool paused, int fadeLength = 0);

    void queueBuffer(const AudioBuffer& buffer);

    void handleTrackChanged();
    void updateOutput(const OutputCreator& output, const QString& device);
    void updateDevice(const QString& device);
    void updateVolume(double volume);

    [[nodiscard]] QString deviceError() const;

signals:
    void paused();
    void outputStateChanged(AudioOutput::State state);
    void bufferProcessed(const Fooyin::AudioBuffer& buffer);
    void finished();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void resetBuffer();
    void resetFade(int length);

    [[nodiscard]] bool canWrite() const;

    bool initOutput();

    [[nodiscard]] bool validOutputState() const;
    void handleStateChanged(AudioOutput::State state);
    void updateOutputVolume(double newVolume);
    void updateInterval();

    void pause();
    void writeNext();
    int writeAudioSamples(int samples);
    int renderAudio(int samples);

    std::unique_ptr<AudioOutput> m_audioOutput;
    AudioFormat m_format;
    double m_volume;
    int m_bufferSize;
    bool m_bufferPrefilled;
    std::unique_ptr<FFmpegResampler> m_resampler;

    std::queue<AudioBuffer> m_bufferQueue;
    AudioBuffer m_tempBuffer;
    int m_totalSamplesWritten;
    int m_currentBufferOffset;

    bool m_isRunning;
    QString m_lastDeviceError;

    QBasicTimer m_writeTimer;
    int m_writeInterval;

    QBasicTimer m_fadeTimer;
    int m_fadeLength;
    int m_fadeSteps;
    int m_currentFadeStep;
    double m_volumeChange;
    double m_initialVolume;
};
} // namespace Fooyin
