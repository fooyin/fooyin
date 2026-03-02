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

#include <core/engine/audioformat.h>

#include <QObject>

#include <cstddef>
#include <span>

namespace Fooyin {
struct OutputState
{
    int freeFrames{0};   // Frames the backend can accept immediately
    int queuedFrames{0}; // Frames queued in backend but not yet played
    double delay{0.0};   // Estimated backend latency in seconds
};

struct OutputDevice
{
    QString name;
    QString desc;
};
using OutputDevices = std::vector<OutputDevice>;

/*!
 * Abstract interface for an audio output backend.
 *
 * Typical lifecycle:
 * 1. `setDevice()` / `negotiateFormat()` (optional)
 * 2. `init(format)`
 * 3. optional prefill writes via `write(...)`
 * 4. `start()`
 * 5. `setPaused(...)` / `write(...)` / `currentState()` during playback
 * 6. `reset()` / `drain()` as needed
 * 7. `uninit()`
 */
class FYCORE_EXPORT AudioOutput : public QObject
{
    Q_OBJECT

public:
    enum class State : uint8_t
    {
        None = 0,
        Error,
        Disconnected
    };

    //! Initialise backend resources for `format`.
    virtual bool init(const AudioFormat& format) = 0;
    /*!
     * Return backend to the pre-`init()` state and release resources.
     * Only called when `initialised()` is true.
     */
    virtual void uninit() = 0;
    /*!
     * Reset runtime playback state while staying initialised.
     * Only called when `initialised()` is true.
     */
    virtual void reset() = 0;
    /*!
     * Drain queued backend frames.
     * Only called when `initialised()` is true.
     */
    virtual void drain() = 0;
    /*!
     * Start hardware/device playback.
     * Only called when `initialised()` is true.
     */
    virtual void start() = 0;

    //! True once `init()` succeeded and before `uninit()`.
    [[nodiscard]] virtual bool initialised() const = 0;
    //! Current device id/name used by backend.
    [[nodiscard]] virtual QString device() const = 0;

    /*!
     * Return current backend queue/latency state.
     * Only called when `initialised()` is true.
     */
    virtual OutputState currentState() = 0;
    /*!
     * Return backend buffer size in frames.
     * Only called when `initialised()` is true.
     */
    [[nodiscard]] virtual int bufferSize() const = 0;
    /*!
     * Enumerate available devices.
     * May be called on helper instances, not necessarily the active one.
     */
    [[nodiscard]] virtual OutputDevices getAllDevices(bool isCurrentOutput) = 0;

    /*!
     * Writes interleaved PCM bytes to the audio driver.
     * @param data Frame-aligned bytes in the output format.
     * @param frameCount Number of frames requested to write from @p data.
     * @note Only called when `initialised()` is true.
     * @note May be called before `start()` to prefill the backend queue.
     * @returns the number of frames written.
     */
    virtual int write(std::span<const std::byte> data, int frameCount) = 0;

    //! Pause/resume backend playback without uninitialising.
    virtual void setPaused(bool pause) = 0;

    /*!
     * Set backend master volume.
     * May be called regardless of current initialised state.
     */
    virtual void setVolume(double volume)
    {
        Q_UNUSED(volume);
    }
    /*!
     * Returns true when the backend applies master volume itself.
     *
     * Backends returning false rely on engine-side post-DSP master scaling.
     */
    [[nodiscard]] virtual bool supportsVolumeControl() const
    {
        return true;
    }

    /*!
     * Select backend device id/name.
     * May be called regardless of current initialised state.
     */
    virtual void setDevice(const QString& device) = 0;

    /*!
     * Optionally negotiate a preferred output layout before init.
     *
     * Implementations should only change metadata that does not require an
     * engine-side resample or remix. The default keeps the requested format.
     */
    [[nodiscard]] virtual AudioFormat negotiateFormat(const AudioFormat& requested) const
    {
        return requested;
    }

    //! Actual output format in use after `init()`.
    [[nodiscard]] virtual AudioFormat format() const = 0;

    //! Last backend error text, if any.
    [[nodiscard]] virtual QString error() const
    {
        return {};
    };

signals:
    //! Backend runtime state transition (`Error`, `Disconnected`, ...).
    void stateChanged(Fooyin::AudioOutput::State state);
};
using OutputCreator = std::function<std::unique_ptr<AudioOutput>()>;
} // namespace Fooyin
