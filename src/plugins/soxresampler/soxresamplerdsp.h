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

#include <soxr.h>

#include <cstdint>
#include <set>
#include <vector>

namespace Fooyin {
class SoxResamplerDSP : public DspNode
{
public:
    enum class Quality : uint8_t
    {
        VeryHigh = 0,
        High,
        Medium,
        Low,
        Quick
    };

    enum class PhaseResponse : uint8_t
    {
        Linear = 0,
        Intermediate,
        Minimum
    };

    enum class SampleRateFilterMode : uint8_t
    {
        ExcludeListed = 0,
        OnlyListed
    };

    SoxResamplerDSP();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString id() const override;

    [[nodiscard]] AudioFormat outputFormat(const AudioFormat& input) const override;
    void prepare(const AudioFormat& format) override;
    void process(ProcessingBufferList& chunks) override;
    void reset() override;
    void flush(ProcessingBufferList& chunks, FlushMode mode) override;
    [[nodiscard]] int latencyFrames() const override;

    [[nodiscard]] QByteArray saveSettings() const override;
    bool loadSettings(const QByteArray& preset) override;

    void setTargetSampleRate(int sampleRate);
    [[nodiscard]] int targetSampleRate() const;

    void setQuality(Quality quality);
    [[nodiscard]] Quality quality() const;

    void setPassbandPercent(double percent);
    [[nodiscard]] double passbandPercent() const;

    void setFullBandwidthUpsampling(bool enable);
    [[nodiscard]] bool fullBandwidthUpsampling() const;

    void setPhaseResponse(PhaseResponse phase);
    [[nodiscard]] PhaseResponse phaseResponse() const;

    void setExcludedSampleRatesText(const QString& text);
    [[nodiscard]] QString excludedSampleRatesText() const;

    void setSampleRateFilterMode(SampleRateFilterMode mode);
    [[nodiscard]] SampleRateFilterMode sampleRateFilterMode() const;

private:
    struct SoxStage
    {
        soxr_t ctx{nullptr};

        int inRate{0};
        int outRate{0};

        double ratio{1.0};
        int latencyFrames{0};

        double passbandEnd{0.0};
        double stopbandBegin{0.0};

        bool hasNextOutTime{false};
        uint64_t nextOutTimeNs{0};
        double nextOutRemainderNs{0.0};
        uint64_t outputSourceFrameDurationNs{0};

        std::vector<double> pendingInput;
        bool hasPendingInputStart{false};
        uint64_t pendingInputStartNs{0};
    };

    static bool createStageCtx(SoxResamplerDSP::Quality quality, SoxResamplerDSP::PhaseResponse phase,
                               double passbandPercent, bool allowImagingUpsampling, int channels, size_t stageIndex,
                               SoxStage& stage);
    static void clearStage(SoxStage& stage);

    void rebuildResampler();
    void destroyResampler();
    bool ensureResampler();

    static size_t estimateOutputFramesForRatio(double ratio, size_t inputFrames);

    bool processOneStageBuffer(SoxStage& stage, const ProcessingBuffer& buffer, ProcessingBufferList& output,
                               const AudioFormat& outFormat);
    void processBuffer(const ProcessingBuffer& buffer, ProcessingBufferList& output);

    void refreshLatencyFramesFromStages();
    [[nodiscard]] int currentPendingInputFrames() const;
    [[nodiscard]] bool shouldResampleInputRate(int inputRate) const;
    [[nodiscard]] std::set<int> parseFilteredSampleRates(const QString& text) const;

    AudioFormat m_format;

    std::vector<SoxStage> m_stages;

    int m_targetRate;
    int m_inputRate;
    int m_channels;
    int m_latencyFrames;

    Quality m_quality;
    double m_passbandPercent;
    bool m_fullBandwidthUpsampling;
    PhaseResponse m_phaseResponse;
    SampleRateFilterMode m_sampleRateFilterMode;
    QString m_excludedSampleRatesText;
    std::set<int> m_filteredSampleRates;

    ProcessingBufferList m_scratchOutput;
    ProcessingBufferList m_scratchA;
    ProcessingBufferList m_scratchB;
    ProcessingBufferList m_scratchFlushInput;
    ProcessingBufferList m_scratchFlushOutput;
    ProcessingBufferList m_scratchFlushDrained;

    uint32_t m_warningCounter;
};
} // namespace Fooyin
