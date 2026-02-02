/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

class QObject;
class QString;

namespace Fooyin {
class AudioPipeline;

/*!
 * Output backend helper for AudioEngine.
 *
 * Encapsulates output creation/device selection and pipeline output lifecycle
 * (init/uninit/pause-reset).
 */
class OutputController
{
public:
    OutputController(QObject* signalTarget, AudioPipeline* pipeline);

    //! Set factory used to create fresh output backend instances.
    void setOutputCreator(const OutputCreator& outputCreator);
    //! Set desired backend device id/name for next init.
    void setOutputDevice(const QString& device);

    [[nodiscard]] const QString& outputDevice() const;

    //! Create backend, attach to pipeline, and initialise for @p format.
    //! Returns false and emits engine error on failure.
    bool initOutput(const AudioFormat& format, double volume);
    //! Uninitialise current pipeline output backend.
    void uninitOutput();
    //! Immediate pause helper used by stop/pause/reconfigure flows.
    void pauseOutputImmediate(bool resetOutput);

private:
    QObject* m_signalTarget;
    AudioPipeline* m_pipeline;
    OutputCreator m_outputCreator;
    QString m_outputDevice;
};
} // namespace Fooyin
