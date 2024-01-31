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

#include "fycore_export.h"

#include <core/engine/audiobuffer.h>
#include <core/engine/audioformat.h>

extern "C"
{
#include <libavutil/channel_layout.h>
}

#include <QString>

namespace Fooyin {
using WriteFunction = std::function<int(uint8_t*, int)>;

struct OutputContext
{
    AudioFormat format;
    AVChannelLayout channelLayout;
    double volume{1.0};

    WriteFunction writeAudioToBuffer;

    bool operator==(const OutputContext& other)
    {
        return std::tie(channelLayout.nb_channels, channelLayout.order, format)
            == std::tie(other.channelLayout.nb_channels, other.channelLayout.order, other.format);
    }
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

/*!
 * An abstract interface for an audio output driver.
 *
 * There are two types of audio driver supported:
 * - Push based: audio is pushed to the driver at regular intervals.
 * - Pull based: a callback is used to write audio to a buffer.
 */
class AudioOutput
{
public:
    virtual ~AudioOutput() = default;

    /** Initialises the output with the given @p oc context. */
    virtual bool init(const OutputContext& oc) = 0;
    /** Resets the output to the state before @fn init was called. */
    virtual void uninit() = 0;
    /** Resets the output to the state after @fn init was called. */
    virtual void reset() = 0;
    /** Starts playback. */
    virtual void start() = 0;

    virtual OutputType type() const = 0;

    /** Returns @c true if the driver was successfully initialised in @fn init. */
    virtual bool initialised() const = 0;
    /** Returns the current driver device being used for playback. */
    virtual QString device() const = 0;

    /*! Returns @c true if the driver can handle volume changes internally.
     *  @note if @c false, a soft-volume will be applied to the samples when required.
     */
    virtual bool canHandleVolume() const = 0;

    virtual OutputState currentState()          = 0;
    virtual int bufferSize() const              = 0;
    virtual OutputDevices getAllDevices() const = 0;

    /*!
     * Writes the audio data contained in the @p buffer to the audio driver.
     * @note for push-based drivers, this may be called before @fn start to prefill the buffer.
     * @returns the number of samples written.
     */
    virtual int write(const AudioBuffer& buffer) = 0;
    virtual void setPaused(bool pause)           = 0;

    /*!
     * Set's the volume of the audio driver.
     * @note this will only be called if @fn canHandleVolume returns true.
     */
    virtual void setVolume(double volume) = 0;

    virtual void setDevice(const QString& device) = 0;
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

    int write(const AudioBuffer& buffer) override;
    void setPaused(bool pause) override;
    void setVolume(double volume) override;
};
} // namespace Fooyin
