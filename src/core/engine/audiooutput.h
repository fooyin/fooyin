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

extern "C"
{
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

#include <QObject>
#include <QString>

namespace Fy::Core::Engine {
struct OutputContext
{
    int sampleRate;
    AVChannelLayout channelLayout;
    AVSampleFormat format;
};

class AudioOutput : public QObject
{
    Q_OBJECT

public:
    AudioOutput(QObject* parent = nullptr)
        : QObject{parent}
    { }
    virtual ~AudioOutput() = default;

    virtual void init(OutputContext* of)          = 0;
    virtual void start()                          = 0;
    virtual int write(const char* data, int size) = 0;

    virtual void setPaused(bool pause) = 0;

    virtual int bufferSize() const       = 0;
    virtual void setBufferSize(int size) = 0;
    virtual void clearBuffer()           = 0;
};
} // namespace Fy::Core::Engine
