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

#include <core/engine/audiobuffer.h>
#include <core/engine/audioformat.h>

#include <QString>

namespace Fooyin {
struct OutputState
{
    int freeSamples{0};
    int queuedSamples{0};
    double delay{0.0};
};

struct OutputDevice
{
    QString name;
    QString desc;
};

using OutputDevices = std::vector<OutputDevice>;

/*!
 * An abstract interface for an audio output driver.
 */
class AudioOutput
{
public:
    virtual ~AudioOutput() = default;

    /** Initialises the output with the given @p format. */
    virtual bool init(const AudioFormat& format) = 0;
    /*!
     *  Resets the output to the state before @fn init was called.
     *  @note this will only be called if @fn initialised returns @c true.
     */
    virtual void uninit() = 0;
    /*!
     *  Resets the output to the state after @fn init was called.
     *  @note this will only be called if @fn initialised returns @c true.
     */
    virtual void reset() = 0;
    /*!
     *  Starts playback.
     *  @note this will only be called if @fn initialised returns @c true.
     */
    virtual void start() = 0;

    /** Returns @c true if the driver was successfully initialised in @fn init. */
    virtual bool initialised() const = 0;
    /** Returns the current driver device being used for playback. */
    virtual QString device() const = 0;

    /*!
     *  Returns @c true if the driver can handle volume changes internally.
     *  @note if @c false, a soft-volume will be applied to the samples when required.
     */
    virtual bool canHandleVolume() const = 0;

    /*!
     *  Returns the current state of the output.
     *  @note this will only be called if @fn initialised returns @c true.
     *
     *  @see OutputState
     */
    virtual OutputState currentState() = 0;
    /*!
     *  Returns the size of the audio driver buffer in samples.
     *  @note this will only be called if @fn initialised returns @c true.
     */
    virtual int bufferSize() const = 0;
    /*!
     *  Returns a list of all device names and descriptions for this driver.
     *  @note this may be called on multiple instances, so don't rely on a
     *  single initialised state.
     */
    virtual OutputDevices getAllDevices() const = 0;

    /*!
     * Writes the audio data contained in the @p buffer to the audio driver.
     * @note this will only be called if @fn initialised returns @c true.
     * @note this may be called before @fn start to prefill the buffer.
     * @returns the number of samples written.
     */
    virtual int write(const AudioBuffer& buffer) = 0;
    virtual void setPaused(bool pause)           = 0;

    /*!
     * Set's the volume of the audio driver.
     * @note this will only be called if @fn canHandleVolume returns @c true.
     * @note this may be called regardless of the current initialised state.
     */
    virtual void setVolume(double /*volume*/){};

    /*!
     * Set's the device for this driver.
     * @note this may be called regardless of the current initialised state.
     */
    virtual void setDevice(const QString& device) = 0;
};
using OutputCreator = std::function<std::unique_ptr<AudioOutput>()>;
} // namespace Fooyin
