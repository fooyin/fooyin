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

#include <QObject>

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

    void updateOutput(const OutputCreator& output, const QString& device);
    void updateDevice(const QString& device);
    void updateVolume(double volume);

signals:
    void paused();
    void outputStateChanged(AudioOutput::State state);
    void bufferProcessed(const AudioBuffer& buffer);
    void finished();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
