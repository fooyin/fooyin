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

#include "supereqadapter.h"

#include <core/engine/dsp/dspnode.h>

#include <array>
#include <cstdint>
#include <vector>

namespace Fooyin::Equaliser {
class EqualiserDsp : public DspNode
{
public:
    static constexpr int BandCount = SuperEq::Processor::BandCount;

    EqualiserDsp();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString id() const override;
    [[nodiscard]] bool supportsLiveSettings() const override;

    [[nodiscard]] AudioFormat outputFormat(const AudioFormat& input) const override;
    void prepare(const AudioFormat& format) override;
    void process(ProcessingBufferList& chunks) override;
    void reset() override;
    void flush(ProcessingBufferList& chunks, FlushMode mode) override;
    [[nodiscard]] int latencyFrames() const override;

    [[nodiscard]] QByteArray saveSettings() const override;
    bool loadSettings(const QByteArray& preset) override;

    void setPreampDb(double value);
    [[nodiscard]] double preampDb() const;

    void setBandDb(int band, double value);
    [[nodiscard]] double bandDb(int band) const;

private:
    struct Settings
    {
        double preampDb{0.0};
        std::array<double, BandCount> bandDb{};
    };

    static Settings defaultSettings();
    static double clampGainDb(double value);
    static bool isNeutralSettings(const Settings& settings);
    static bool settingsEqual(const Settings& lhs, const Settings& rhs);

    void publishSettings(const Settings& settings);
    bool applySettingsIfNeeded();
    void ensurePreparedFor(const AudioFormat& format);

    void emitOutputChunk(ProcessingBufferList& output, int frames, uint64_t sourceFrameDurationNs);
    void advanceOutputCursor(int frames, uint64_t sourceFrameDurationNs);

    AudioFormat m_format;
    SuperEq::Processor m_processor;

    std::vector<double> m_outputScratch;

    Settings m_settings;
    Settings m_appliedSettings;
    bool m_hasAppliedSettings;
    bool m_settingsDirty;

    bool m_hasOutputCursor;
    uint64_t m_outputCursorNs;
    double m_outputCursorRemainderNs;
    uint64_t m_outputSourceFrameDurationNs;
};
} // namespace Fooyin::Equaliser
