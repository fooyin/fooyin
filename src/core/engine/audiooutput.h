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

#include <QString>
extern "C"
{
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

#include <map>

namespace Fy::Core::Engine {
struct OutputContext
{
    int sampleRate;
    AVChannelLayout channelLayout;
    AVSampleFormat format;
    int sstride; // Size of a sample
    int bufferSize{2048};
};

struct OutputState
{
    int freeSamples{0};
    int queuedSamples{0};
    double delay{0.0};
};

using OutputDevices = std::map<QString, QString>;

class AudioOutput
{
public:
    virtual ~AudioOutput() = default;

    virtual QString name() const = 0;

    virtual QString device() const                = 0;
    virtual void setDevice(const QString& device) = 0;

    virtual bool init(OutputContext* oc) = 0;
    virtual void uninit()                = 0;
    virtual void reset()                 = 0;
    virtual void start()                 = 0;

    virtual int write(OutputContext* oc, const uint8_t* data, int samples) = 0;

    virtual void setPaused(bool pause)                  = 0;
    virtual OutputState currentState(OutputContext* oc) = 0;

    virtual OutputDevices getAllDevices() const = 0;
};
} // namespace Fy::Core::Engine
