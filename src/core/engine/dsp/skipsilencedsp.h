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

#include <core/engine/dsp/dspnode.h>
#include <core/engine/dsp/processingbuffer.h>

#include <atomic>
#include <cstdint>

namespace Fooyin {
class SkipSilenceDsp : public DspNode
{
public:
    SkipSilenceDsp();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString id() const override;

    void prepare(const AudioFormat& format) override;
    void process(ProcessingBufferList& chunks) override;
    void reset() override;
    void flush(ProcessingBufferList& chunks, FlushMode mode) override;

    [[nodiscard]] int minSilenceDurationMs() const;
    void setMinSilenceDurationMs(int durationMs);

    [[nodiscard]] int thresholdDb() const;
    void setThresholdDb(int thresholdDb);

    [[nodiscard]] bool keepInitialPeriod() const;
    void setKeepInitialPeriod(bool keep);

    [[nodiscard]] bool includeMiddleSilence() const;
    void setIncludeMiddleSilence(bool include);

    [[nodiscard]] QByteArray saveSettings() const override;
    bool loadSettings(const QByteArray& preset) override;

private:
    struct PendingSilence
    {
        bool active{false};
        bool leading{false};
        bool preserve{false};
        bool drop{false};
        int64_t frames{0};
        uint64_t startTimeNs{0};
        ProcessingBuffer buffer;

        void reset()
        {
            active      = false;
            leading     = false;
            preserve    = false;
            drop        = false;
            frames      = 0;
            startTimeNs = 0;
            buffer.reset();
        }
    };

    void processBuffer(const ProcessingBuffer& buffer, ProcessingBufferList& output);
    void appendPendingSilence(const AudioFormat& format, const double* data, int frames, int channels,
                              uint64_t startTimeNs);
    void finalisePendingSilence(ProcessingBufferList& output, int minFrames);
    void emitSegment(ProcessingBufferList& output, const AudioFormat& format, const double* data, int frames,
                     int channels, uint64_t startTimeNs);
    [[nodiscard]] bool isSilentFrame(const double* frame, int channels, double threshold) const;

    AudioFormat m_format;

    std::atomic<int> m_minSilenceDurationMs;
    std::atomic<int> m_thresholdDb;
    std::atomic<bool> m_keepInitialPeriod;
    std::atomic<bool> m_includeMiddleSilence;

    PendingSilence m_pendingSilence;
    bool m_haveSeenNonSilence;
};
} // namespace Fooyin
