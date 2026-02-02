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

#include "fycore_export.h"

#include <core/engine/audioformat.h>
#include <core/engine/dsp/processingbufferlist.h>

#include <cstdint>

namespace Fooyin {
/*!
 * Base class for DSP processing nodes used by `DspChain`.
 *
 * Lifecycle for a node instance is typically:
 * - construct
 * - optional `loadSettings()`
 * - `prepare(inputFormat)`
 * - repeated `process()` calls
 * - optional `flush()` / `reset()` during discontinuities
 *
 * Threading contract:
 * - The runtime chain is single-owner and driven from the audio thread.
 * - `prepare()`, `process()`, `flush()`, and `reset()` are not called
 *   concurrently on the same instance.
 * - `loadSettings()` may be called on non-audio threads during configuration.
 * - If `supportsLiveSettings()` returns true, `loadSettings()` may also be
 *   called on the audio thread while playback is active.
 */
class FYCORE_EXPORT DspNode
{
public:
    struct Entry
    {
        QString id;
        QString name;
        std::function<std::unique_ptr<DspNode>()> factory;
    };

    enum class FlushMode : uint8_t
    {
        //! End-of-track: emit any delayed tail if applicable.
        EndOfTrack,
        //! Discontinuity/stop: discard buffered state immediately.
        Flush
    };

    DspNode();
    virtual ~DspNode() = default;

    //! Human-readable name for this DSP
    [[nodiscard]] virtual QString name() const = 0;

    //! Unique identifier for serialisation
    [[nodiscard]] virtual QString id() const = 0;
    //! Return true if `loadSettings()` is safe to apply during playback.
    [[nodiscard]] virtual bool supportsLiveSettings() const;

    /*!
     * Prepare node state for a new input format.
     *
     * Called before `process()` whenever the upstream format topology changes.
     * Reallocate/reconfigure internal state here.
     */
    virtual void prepare(const AudioFormat& format) = 0;

    /*!
     * Predict the output format for a given input format.
     *
     * This should be a pure format mapping with no side effects. The buffers
     * produced by `process()` should match this format mapping.
     */
    [[nodiscard]] virtual AudioFormat outputFormat(const AudioFormat& input) const;

    /*!
     * Process audio buffers.
     *
     * Nodes may mutate, remove, or insert chunks. Output chunks should keep
     * timeline continuity (monotonic start times) and conform to the format
     * declared by `outputFormat()`.
     *
     * Called from the audio thread during playback, so implementations should
     * avoid blocking operations.
     */
    virtual void process(ProcessingBufferList& chunks) = 0;

    /*!
     * Reset internal state (delay lines/history/cursors).
     *
     * Called when playback continuity is broken (seek, stream switch, etc).
     */
    virtual void reset();

    /*!
     * Flush buffered state according to `mode`.
     *
     * For `FlushMode::EndOfTrack`, nodes should append any delayed tail they
     * need to output. For `FlushMode::Flush`, nodes should drop buffered state
     * without synthesizing tail output.
     */
    virtual void flush(ProcessingBufferList& chunks, FlushMode mode);

    /*!
     * Save/restore DSP settings payload.
     *
     * `saveSettings()` should return a self-contained payload accepted by
     * `loadSettings()`. `loadSettings()` should validate input and return false
     * when deserialisation fails.
     */
    [[nodiscard]] virtual QByteArray saveSettings() const;
    virtual bool loadSettings(const QByteArray& preset);

    /*!
     * Report latency introduced by this node in frames.
     *
     * Frame units are expressed in the node input sample-rate domain
     * (the format passed to `prepare()` / current upstream format).
     *
     * Used by the engine for playback delay compensation.
     */
    [[nodiscard]] virtual int latencyFrames() const;

    [[nodiscard]] uint64_t instanceId() const;
    void setInstanceId(uint64_t instanceId);

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;

private:
    uint64_t m_instanceId;
    bool m_enabled;
};

using DspNodePtr = std::unique_ptr<DspNode>;
}; // namespace Fooyin
