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

#include "fycore_export.h"

#include <QString>
extern "C"
{
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

namespace Fy::Core::Engine {
using WriteFunction = std::function<int(uint8_t*, int)>;

struct OutputContext
{
    int sampleRate;
    AVChannelLayout channelLayout;
    AVSampleFormat format;
    int sstride; // Size of a sample
    double volume{1.0};

    WriteFunction writeAudioToBuffer;
};

struct OutputState
{
    int freeSamples{0};
    int queuedSamples{0};
    double delay{0.0};
};

enum class OutputType
{
    Push,
    Pull
};

struct OutputDevice
{
    QString name;
    QString desc;
};

using OutputDevices = std::vector<OutputDevice>;

class FYCORE_EXPORT AudioOutput
{
public:
    virtual ~AudioOutput() = default;

    virtual bool init(const OutputContext& oc) = 0;
    virtual void uninit()                      = 0;
    virtual void reset()                       = 0;
    virtual void start()                       = 0;

    virtual OutputType type() const             = 0;
    virtual bool initialised() const            = 0;
    virtual QString device() const              = 0;
    virtual bool canHandleVolume()  const               = 0;
    virtual OutputState currentState()          = 0;
    virtual int bufferSize() const              = 0;
    virtual OutputDevices getAllDevices() const = 0;

    virtual int write(const uint8_t* data, int samples) = 0;
    virtual void setPaused(bool pause)                  = 0;
    virtual void setVolume(double volume)               = 0;
    virtual void setDevice(const QString& device)       = 0;
};

class FYCORE_EXPORT AudioPushOutput : public AudioOutput
{
public:
    OutputType type() const override;

    void setPaused(bool pause) override;
    void setVolume(double volume) override;
};

class FYCORE_EXPORT AudioPullOutput : public AudioOutput
{
public:
    void reset() override;

    OutputType type() const override;
    OutputState currentState() override;
    int bufferSize() const override;

    int write(const uint8_t* data, int samples) override;
    void setPaused(bool pause) override;
    void setVolume(double volume) override;
};
} // namespace Fy::Core::Engine
