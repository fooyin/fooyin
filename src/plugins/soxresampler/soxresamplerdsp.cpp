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

#include "soxresamplerdsp.h"

#include <utils/timeconstants.h>

#include <QDataStream>
#include <QIODevice>
#include <QLoggingCategory>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

Q_LOGGING_CATEGORY(SOXR_RESAMPLER, "fy.soxresampler")

constexpr auto PresetVersion              = 1;
constexpr auto MaxSampleRate              = 384000;
constexpr auto MinSampleRate              = 0;
constexpr auto MinTargetSampleRate        = 1;
constexpr auto DefaultPassbandPercent     = 95.0;
constexpr auto DefaultExcludedSampleRates = "";

constexpr size_t OutputPaddingFrames     = 64;
constexpr size_t ProcessPaddingFrames    = 256;
constexpr auto MaxProcessPaddingFrames   = 65536;
constexpr auto BaseMaxProcessIterations  = 1024;
constexpr auto MaxProcessIterationsCap   = 32768;
constexpr auto MaxTinyProgressIterations = 8192;

constexpr auto WarningBurstCount  = 8;
constexpr auto WarningLogInterval = 128;

constexpr auto Nyq        = 1.0;
constexpr auto SoxrMinFp  = 0.5;
constexpr auto SoxrMaxFs  = 1.5;
constexpr auto SoxrMinTbw = 0.002;

namespace {
uint64_t nominalFrameDurationNs(const int sampleRate)
{
    const int safeRate   = std::max(1, sampleRate);
    const double frameNs = static_cast<double>(Fooyin::Time::NsPerSecond) / static_cast<double>(safeRate);
    return static_cast<uint64_t>(std::max<int64_t>(1, std::llround(frameNs)));
}

uint64_t sourceFrameDurationNsForBuffer(const Fooyin::ProcessingBuffer& buffer, const int sampleRate)
{
    return buffer.sourceFrameDurationNs() > 0 ? buffer.sourceFrameDurationNs() : nominalFrameDurationNs(sampleRate);
}

uint64_t mappedSourceFrameDurationNs(const uint64_t inputSourceFrameDurationNs, const int inRate, const int outRate)
{
    const double mappedNs = (static_cast<double>(inputSourceFrameDurationNs) * static_cast<double>(std::max(1, inRate)))
                          / static_cast<double>(std::max(1, outRate));
    return static_cast<uint64_t>(std::max<int64_t>(1, std::llround(mappedNs)));
}

void advanceSourceCursor(uint64_t& cursorNs, double& cursorRemainderNs, const int frames,
                         const uint64_t sourceFrameDurationNs)
{
    if(frames <= 0 || sourceFrameDurationNs == 0) {
        return;
    }

    const double advanceNsFull
        = (static_cast<double>(frames) * static_cast<double>(sourceFrameDurationNs)) + cursorRemainderNs;
    const auto advanceNs = static_cast<uint64_t>(std::max(0.0, std::floor(advanceNsFull)));
    cursorRemainderNs    = advanceNsFull - static_cast<double>(advanceNs);
    cursorNs += advanceNs;
}

void configureBands(soxr_quality_spec_t& q, double bandwidthPercent, int inputRate, int targetRate,
                    bool allowImagingUpsampling)
{
    const bool isUpsampling = targetRate > inputRate;

    const double fpReq = std::max(std::clamp(bandwidthPercent / 100.0, 0.0, Nyq), SoxrMinFp);

    if(isUpsampling && allowImagingUpsampling) {
        const double fp  = std::min(fpReq, Nyq - SoxrMinTbw);
        const double fs  = std::clamp(Nyq + SoxrMinTbw, fp + SoxrMinTbw, SoxrMaxFs);
        q.passband_end   = fp;
        q.stopband_begin = fs;
        return;
    }

    q.stopband_begin = Nyq;
    q.passband_end   = std::min(fpReq, Nyq - SoxrMinTbw);
}
} // namespace

namespace Fooyin {
namespace {
unsigned qualityRecipe(SoxResamplerDSP::Quality quality)
{
    switch(quality) {
        case SoxResamplerDSP::Quality::VeryHigh:
            return SOXR_VHQ;
        case SoxResamplerDSP::Quality::High:
            return SOXR_HQ;
        case SoxResamplerDSP::Quality::Medium:
            return SOXR_MQ;
        case SoxResamplerDSP::Quality::Low:
            return SOXR_LQ;
        case SoxResamplerDSP::Quality::Quick:
        default:
            return SOXR_QQ;
    }
}

double qualityPrecision(SoxResamplerDSP::Quality quality)
{
    switch(quality) {
        case SoxResamplerDSP::Quality::VeryHigh:
            return 28.0;
        case SoxResamplerDSP::Quality::High:
            return 20.0;
        case SoxResamplerDSP::Quality::Medium:
            return 18.0;
        case SoxResamplerDSP::Quality::Low:
            return 16.0;
        case SoxResamplerDSP::Quality::Quick:
        default:
            return 15.0;
    }
}

SoxResamplerDSP::Quality qualityFromIndex(int index)
{
    switch(index) {
        case 0:
            return (SoxResamplerDSP::Quality::VeryHigh);
        case 1:
            return (SoxResamplerDSP::Quality::High);
        case 2:
            return (SoxResamplerDSP::Quality::Medium);
        case 3:
            return (SoxResamplerDSP::Quality::Low);
        case 4:
            return (SoxResamplerDSP::Quality::Quick);
        default:
            return (SoxResamplerDSP::Quality::VeryHigh);
    }
}

int qualityToIndex(SoxResamplerDSP::Quality quality)
{
    switch(quality) {
        case SoxResamplerDSP::Quality::VeryHigh:
            return 0;
        case SoxResamplerDSP::Quality::High:
            return 1;
        case SoxResamplerDSP::Quality::Medium:
            return 2;
        case SoxResamplerDSP::Quality::Low:
            return 3;
        case SoxResamplerDSP::Quality::Quick:
        default:
            return 4;
    }
}

SoxResamplerDSP::PhaseResponse phaseResponseFromIndex(int index)
{
    switch(index) {
        case 1:
            return SoxResamplerDSP::PhaseResponse::Intermediate;
        case 2:
            return SoxResamplerDSP::PhaseResponse::Minimum;
        case 0:
        default:
            return SoxResamplerDSP::PhaseResponse::Linear;
    }
}

int phaseResponseToIndex(SoxResamplerDSP::PhaseResponse phase)
{
    switch(phase) {
        case SoxResamplerDSP::PhaseResponse::Linear:
            return 0;
        case SoxResamplerDSP::PhaseResponse::Intermediate:
            return 1;
        case SoxResamplerDSP::PhaseResponse::Minimum:
        default:
            return 2;
    }
}

double phaseResponseValue(SoxResamplerDSP::PhaseResponse phase)
{
    switch(phase) {
        case SoxResamplerDSP::PhaseResponse::Linear:
            return 50.0;
        case SoxResamplerDSP::PhaseResponse::Intermediate:
            return 25.0;
        case SoxResamplerDSP::PhaseResponse::Minimum:
        default:
            return 0.0;
    }
}

SoxResamplerDSP::SampleRateFilterMode sampleRateFilterModeFromIndex(int index)
{
    switch(index) {
        case 1:
            return SoxResamplerDSP::SampleRateFilterMode::OnlyListed;
        case 0:
        default:
            return SoxResamplerDSP::SampleRateFilterMode::ExcludeListed;
    }
}

int sampleRateFilterModeToIndex(SoxResamplerDSP::SampleRateFilterMode mode)
{
    switch(mode) {
        case SoxResamplerDSP::SampleRateFilterMode::OnlyListed:
            return 1;
        case SoxResamplerDSP::SampleRateFilterMode::ExcludeListed:
        default:
            return 0;
    }
}

bool shouldLogWarning(uint32_t& counter)
{
    ++counter;
    return counter <= WarningBurstCount || (counter % WarningLogInterval) == 0;
}

double normalisedToHz(const double normalisedNy, const int sampleRate)
{
    if(sampleRate <= 0) {
        return 0.0;
    }

    return normalisedNy * (static_cast<double>(sampleRate) * 0.5);
}
} // namespace

SoxResamplerDSP::SoxResamplerDSP()
    : m_targetRate{48000}
    , m_inputRate{0}
    , m_channels{0}
    , m_latencyFrames{0}
    , m_quality{Quality::VeryHigh}
    , m_passbandPercent{DefaultPassbandPercent}
    , m_fullBandwidthUpsampling{false}
    , m_phaseResponse{PhaseResponse::Linear}
    , m_sampleRateFilterMode{SampleRateFilterMode::ExcludeListed}
    , m_warningCounter{0}
{ }

QString SoxResamplerDSP::name() const
{
    const QString baseName = QStringLiteral("Resampler (SoX)");
    return QStringLiteral("%1: %2 Hz").arg(baseName).arg(m_targetRate);
}

QString SoxResamplerDSP::id() const
{
    return QStringLiteral("fooyin.dsp.soxresampler");
}

AudioFormat SoxResamplerDSP::outputFormat(const AudioFormat& input) const
{
    if(!input.isValid()) {
        return {};
    }

    if(!shouldResampleInputRate(input.sampleRate())) {
        return input;
    }

    AudioFormat out = input;
    out.setSampleRate(m_targetRate);
    return out;
}

void SoxResamplerDSP::prepare(const AudioFormat& format)
{
    m_format = format;
    rebuildResampler();
}

void SoxResamplerDSP::reset()
{
    for(auto& s : m_stages) {
        clearStage(s);
    }
    m_latencyFrames = 0;
}

void SoxResamplerDSP::flush(ProcessingBufferList& chunks, FlushMode mode)
{
    if(mode == FlushMode::Flush) {
        reset();
        return;
    }

    if(!m_format.isValid() || !shouldResampleInputRate(m_format.sampleRate())) {
        reset();
        return;
    }

    if(!ensureResampler()) {
        reset();
        return;
    }

    if(m_stages.empty()) {
        return;
    }

    uint64_t tailTimeNs{0};
    if(chunks.count() > 0) {
        if(const auto* last = chunks.item(chunks.count() - 1)) {
            tailTimeNs = last->endTimeNs();
        }
    }

    auto appendValidChunks = [](const ProcessingBufferList& src, ProcessingBufferList& dst) {
        for(size_t i{0}; i < src.count(); ++i) {
            const auto* b = src.item(i);
            if(b && b->isValid() && b->frameCount() > 0) {
                dst.addChunk(*b);
            }
        }
    };

    auto listEndTime = [](const ProcessingBufferList& list, uint64_t fallbackNs) {
        uint64_t endNs = fallbackNs;
        for(size_t i{0}; i < list.count(); ++i) {
            if(const auto* b = list.item(i); b && b->isValid() && b->frameCount() > 0) {
                if(b->sourceFrameDurationNs() > 0) {
                    endNs = b->startTimeNs() + (static_cast<uint64_t>(b->frameCount()) * b->sourceFrameDurationNs());
                }
                else {
                    endNs = b->endTimeNs();
                }
            }
        }
        return endNs;
    };

    // Forward-only tail propagation
    // stageInput carries pending tail at current stage input rate
    // For each stage, process stageInput, then drain that stage, and pass
    // combined output to the next stage

    auto& stageInput  = m_scratchFlushInput;
    auto& stageOutput = m_scratchFlushOutput;
    auto& drained     = m_scratchFlushDrained;

    stageInput.clear();
    stageOutput.clear();
    drained.clear();

    for(auto& stage : m_stages) {
        if(!stage.ctx) {
            stageInput.clear();
            continue;
        }

        AudioFormat stageOutFmt{m_format};
        stageOutFmt.setSampleRate(stage.outRate);

        stageOutput.clear();

        bool stageOk{true};

        for(size_t i{0}; i < stageInput.count(); ++i) {
            const auto* inBuf = stageInput.item(i);
            if(!inBuf || !inBuf->isValid() || inBuf->frameCount() <= 0) {
                continue;
            }
            if(!processOneStageBuffer(stage, *inBuf, stageOutput, stageOutFmt)) {
                stageOk = false;
                break;
            }
        }

        stageInput.clear();

        uint64_t stageTime      = stage.hasNextOutTime ? stage.nextOutTimeNs : listEndTime(stageOutput, tailTimeNs);
        double stageRemainderNs = stage.hasNextOutTime ? stage.nextOutRemainderNs : 0.0;
        const uint64_t stageSourceFrameDurationNs = stage.outputSourceFrameDurationNs > 0
                                                      ? stage.outputSourceFrameDurationNs
                                                      : nominalFrameDurationNs(stage.outRate);
        drained.clear();

        static constexpr auto maxDrainIters = 32;

        for(int iter{0}; iter < maxDrainIters; ++iter) {
            const size_t outCapacity
                = std::max<size_t>(1, static_cast<size_t>(stage.latencyFrames + ProcessPaddingFrames));
            const size_t outSamples = outCapacity * static_cast<size_t>(m_channels);

            auto* outBuffer = drained.addItem(stageOutFmt, stageTime, outSamples);
            if(!outBuffer) {
                stageOk = false;
                break;
            }

            auto outSpan = outBuffer->data();
            if(outSpan.size() < outSamples) {
                drained.removeByIdx(drained.count() - 1);
                stageOk = false;
                break;
            }

            size_t odone{0};
            const soxr_error_t err = soxr_process(stage.ctx, nullptr, 0, nullptr, outSpan.data(), outCapacity, &odone);

            if(err || odone == 0) {
                if(err) {
                    if(shouldLogWarning(m_warningCounter)) {
                        qCWarning(SOXR_RESAMPLER) << "SoXR drain failed:" << err << "stageIn=" << stage.inRate
                                                  << "stageOut=" << stage.outRate << "iter=" << iter;
                    }
                }
                drained.removeByIdx(drained.count() - 1);
                break;
            }

            outBuffer->resizeSamples(odone * static_cast<size_t>(m_channels));
            outBuffer->setSourceFrameDurationNs(stageSourceFrameDurationNs);
            advanceSourceCursor(stageTime, stageRemainderNs, static_cast<int>(odone), stageSourceFrameDurationNs);
        }

        stage.nextOutTimeNs      = stageTime;
        stage.nextOutRemainderNs = stageRemainderNs;

        // End-of-track flush intentionally clears stage history once drained
        clearStage(stage);

        if(!stageOk) {
            if(shouldLogWarning(m_warningCounter)) {
                qCWarning(SOXR_RESAMPLER) << "SoXR stage flush failed; dropping remaining tail"
                                          << "stageIn=" << stage.inRate << "stageOut=" << stage.outRate;
            }
            stageOutput.clear();
            drained.clear();
        }

        appendValidChunks(drained, stageOutput);
        tailTimeNs = listEndTime(stageOutput, tailTimeNs);

        appendValidChunks(stageOutput, stageInput);
        stageOutput.clear();
        drained.clear();
    }

    appendValidChunks(stageInput, chunks);
    stageInput.clear();
    stageOutput.clear();
    drained.clear();
    refreshLatencyFramesFromStages();
}

int SoxResamplerDSP::latencyFrames() const
{
    return currentPendingInputFrames();
}

void SoxResamplerDSP::process(ProcessingBufferList& chunks)
{
    if(!m_format.isValid() || !shouldResampleInputRate(m_format.sampleRate())) {
        m_latencyFrames = 0;
        return;
    }

    if(!ensureResampler()) {
        m_latencyFrames = 0;
        return;
    }

    auto& output   = m_scratchOutput;
    auto& inputRef = chunks;

    output.clear();

    const size_t count = inputRef.count();
    if(count == 0) {
        refreshLatencyFramesFromStages();
        chunks.clear();
        return;
    }

    for(size_t i{0}; i < count; ++i) {
        const auto* buffer = inputRef.item(i);
        if(!buffer || !buffer->isValid() || buffer->frameCount() <= 0) {
            continue;
        }
        processBuffer(*buffer, output);
    }

    refreshLatencyFramesFromStages();
    chunks.clear();

    const size_t outCount = output.count();
    for(size_t i{0}; i < outCount; ++i) {
        const auto* buffer = output.item(i);
        if(buffer && buffer->isValid()) {
            chunks.addChunk(*buffer);
        }
    }
}

QByteArray SoxResamplerDSP::saveSettings() const
{
    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<quint32>(PresetVersion);
    stream << static_cast<qint32>(m_targetRate);
    stream << static_cast<qint32>(m_quality);
    stream << m_passbandPercent;
    stream << m_fullBandwidthUpsampling;
    stream << static_cast<qint32>(m_phaseResponse);
    stream << m_excludedSampleRatesText;
    stream << static_cast<qint32>(m_sampleRateFilterMode);

    return data;
}

bool SoxResamplerDSP::loadSettings(const QByteArray& preset)
{
    if(preset.isEmpty()) {
        return false;
    }

    QDataStream stream{preset};
    stream.setVersion(QDataStream::Qt_6_0);

    quint32 version{0};
    stream >> version;

    if(stream.status() != QDataStream::Ok) {
        return false;
    }

    if(version != PresetVersion) {
        qCWarning(SOXR_RESAMPLER) << "Unsupported SoXR preset version:" << version << "(expected" << PresetVersion
                                  << ")";
        return false;
    }

    qint32 targetRate{0};
    qint32 quality = qualityToIndex(Quality::VeryHigh);
    double passband{DefaultPassbandPercent};
    bool fullBandwidth{false};
    qint32 phase         = phaseResponseToIndex(PhaseResponse::Linear);
    QString excludedText = QString::fromLatin1(DefaultExcludedSampleRates);
    qint32 filterMode    = sampleRateFilterModeToIndex(SampleRateFilterMode::ExcludeListed);

    stream >> targetRate >> quality >> passband >> fullBandwidth >> phase >> excludedText >> filterMode;

    if(stream.status() != QDataStream::Ok) {
        return false;
    }

    if(targetRate <= 0) {
        targetRate = 48000;
    }

    setTargetSampleRate(targetRate);
    setQuality(qualityFromIndex(quality));
    setPassbandPercent(passband);
    setFullBandwidthUpsampling(fullBandwidth);
    setPhaseResponse(phaseResponseFromIndex(phase));
    setExcludedSampleRatesText(excludedText);
    setSampleRateFilterMode(sampleRateFilterModeFromIndex(filterMode));

    return true;
}

void SoxResamplerDSP::setTargetSampleRate(int sampleRate)
{
    sampleRate = std::clamp(sampleRate, MinTargetSampleRate, MaxSampleRate);
    if(m_targetRate == sampleRate) {
        return;
    }

    m_targetRate = sampleRate;
    rebuildResampler();
}

int SoxResamplerDSP::targetSampleRate() const
{
    return m_targetRate;
}

void SoxResamplerDSP::setQuality(Quality quality)
{
    if(m_quality == quality) {
        return;
    }

    m_quality = quality;
    rebuildResampler();
}

SoxResamplerDSP::Quality SoxResamplerDSP::quality() const
{
    return m_quality;
}

void SoxResamplerDSP::setPassbandPercent(double percent)
{
    if(std::abs(m_passbandPercent - percent) < 0.0001) {
        return;
    }

    m_passbandPercent = percent;
    rebuildResampler();
}

double SoxResamplerDSP::passbandPercent() const
{
    return m_passbandPercent;
}

void SoxResamplerDSP::setFullBandwidthUpsampling(bool enable)
{
    if(m_fullBandwidthUpsampling == enable) {
        return;
    }

    m_fullBandwidthUpsampling = enable;
    rebuildResampler();
}

bool SoxResamplerDSP::fullBandwidthUpsampling() const
{
    return m_fullBandwidthUpsampling;
}

void SoxResamplerDSP::setPhaseResponse(PhaseResponse phase)
{
    if(m_phaseResponse == phase) {
        return;
    }

    m_phaseResponse = phase;
    rebuildResampler();
}

SoxResamplerDSP::PhaseResponse SoxResamplerDSP::phaseResponse() const
{
    return m_phaseResponse;
}

void SoxResamplerDSP::setExcludedSampleRatesText(const QString& text)
{
    const QString trimmed = text.trimmed();
    std::set<int> rates   = parseFilteredSampleRates(trimmed);

    if(m_excludedSampleRatesText == trimmed && m_filteredSampleRates == rates) {
        return;
    }

    m_excludedSampleRatesText = trimmed;
    m_filteredSampleRates     = std::move(rates);
    rebuildResampler();
}

QString SoxResamplerDSP::excludedSampleRatesText() const
{
    return m_excludedSampleRatesText;
}

void SoxResamplerDSP::setSampleRateFilterMode(SampleRateFilterMode mode)
{
    if(m_sampleRateFilterMode == mode) {
        return;
    }

    m_sampleRateFilterMode = mode;
    rebuildResampler();
}

void SoxResamplerDSP::clearStage(SoxStage& stage)
{
    if(stage.ctx) {
        soxr_clear(stage.ctx);
    }

    stage.latencyFrames               = 0;
    stage.passbandEnd                 = 0.0;
    stage.stopbandBegin               = 0.0;
    stage.hasNextOutTime              = false;
    stage.nextOutTimeNs               = 0;
    stage.nextOutRemainderNs          = 0.0;
    stage.outputSourceFrameDurationNs = 0;
}

bool SoxResamplerDSP::createStageCtx(Quality quality, PhaseResponse phase, double passbandPercent,
                                     bool allowImagingUpsampling, int channels, size_t stageIndex, SoxStage& stage)
{
    soxr_error_t error{nullptr};
    const soxr_io_spec_t ioSpec     = soxr_io_spec(SOXR_FLOAT64_I, SOXR_FLOAT64_I);
    soxr_quality_spec_t qualitySpec = soxr_quality_spec(qualityRecipe(quality), 0);
    qualitySpec.precision           = qualityPrecision(quality);
    qualitySpec.phase_response      = phaseResponseValue(phase);

    configureBands(qualitySpec, passbandPercent, stage.inRate, stage.outRate, allowImagingUpsampling);

    stage.passbandEnd   = qualitySpec.passband_end;
    stage.stopbandBegin = qualitySpec.stopband_begin;

    stage.ctx = soxr_create(static_cast<double>(stage.inRate), static_cast<double>(stage.outRate),
                            static_cast<unsigned>(channels), &error, &ioSpec, &qualitySpec, nullptr);

    if(error || !stage.ctx) {
        qCWarning(SOXR_RESAMPLER) << "Failed to create SoXR stage:" << (error ? error : "unknown error")
                                  << "inRate=" << stage.inRate << "outRate=" << stage.outRate << "channels=" << channels
                                  << "passbandEnd=" << qualitySpec.passband_end
                                  << "stopbandBegin=" << qualitySpec.stopband_begin
                                  << "passbandHz=" << normalisedToHz(qualitySpec.passband_end, stage.inRate)
                                  << "stopbandHz=" << normalisedToHz(qualitySpec.stopband_begin, stage.inRate)
                                  << "stageIndex=" << stageIndex;
        if(stage.ctx) {
            soxr_delete(stage.ctx);
            stage.ctx = nullptr;
        }
        return false;
    }

    const double delay  = soxr_delay(stage.ctx);
    stage.latencyFrames = delay > 0.0 ? static_cast<int>(std::ceil(delay)) : 0;
    stage.ratio         = static_cast<double>(stage.outRate) / static_cast<double>(stage.inRate);

    return true;
}

SoxResamplerDSP::SampleRateFilterMode SoxResamplerDSP::sampleRateFilterMode() const
{
    return m_sampleRateFilterMode;
}

bool SoxResamplerDSP::shouldResampleInputRate(int inputRate) const
{
    if(inputRate <= 0 || inputRate == m_targetRate) {
        return false;
    }

    const bool listed = m_filteredSampleRates.contains(inputRate);

    switch(m_sampleRateFilterMode) {
        case SampleRateFilterMode::ExcludeListed:
            return !listed;
        case SampleRateFilterMode::OnlyListed:
            return listed;
    }

    return true;
}

std::set<int> SoxResamplerDSP::parseFilteredSampleRates(const QString& text) const
{
    std::set<int> rates;
    QString token;

    const auto parseToken = [&rates](const QString& value) {
        bool ok        = false;
        const int rate = value.toInt(&ok);
        if(ok && rate > MinSampleRate && rate <= MaxSampleRate) {
            rates.insert(rate);
        }
    };

    for(const QChar c : text) {
        if(c.isDigit()) {
            token.append(c);
        }
        else if(!token.isEmpty()) {
            parseToken(token);
            token.clear();
        }
    }

    if(!token.isEmpty()) {
        parseToken(token);
    }

    return rates;
}

void SoxResamplerDSP::destroyResampler()
{
    for(auto& s : m_stages) {
        if(s.ctx) {
            soxr_delete(s.ctx);
            s.ctx = nullptr;
        }
    }
    m_stages.clear();

    m_latencyFrames = 0;
    m_inputRate     = 0;
    m_channels      = 0;
}

void SoxResamplerDSP::rebuildResampler()
{
    destroyResampler();

    if(!m_format.isValid()) {
        return;
    }

    m_inputRate = m_format.sampleRate();
    m_channels  = m_format.channelCount();

    if(m_inputRate <= 0 || m_channels <= 0) {
        return;
    }

    if(!shouldResampleInputRate(m_inputRate)) {
        return;
    }

    const bool isUpsampling = m_targetRate > m_inputRate;
    const bool allowImaging = m_fullBandwidthUpsampling && isUpsampling;

    // Use multistage planning only for large upsampling ratios (>2x)
    const bool useMultistage = isUpsampling && (m_targetRate > (2 * m_inputRate));

    std::vector<SoxStage> plan;
    size_t stageReserve{1};

    if(useMultistage) {
        const auto maxRate = static_cast<double>(std::max(m_inputRate, m_targetRate));
        const auto minRate = static_cast<double>(std::max(1, std::min(m_inputRate, m_targetRate)));
        const double ratio = maxRate / minRate;
        stageReserve       = std::max<size_t>(2, static_cast<size_t>(std::ceil(std::log2(ratio))));
    }

    plan.reserve(stageReserve);

    const auto makeStage = [](const int inRate, const int outRate) {
        SoxStage s;
        s.inRate  = inRate;
        s.outRate = outRate;
        return s;
    };

    if(useMultistage) {
        int rate{m_inputRate};

        if(isUpsampling) {
            while(m_targetRate > 2 * rate) {
                const int nextRate = rate * 2;
                plan.push_back(makeStage(rate, nextRate));
                rate = nextRate;
            }
        }
        else {
            while(rate > 2 * m_targetRate) {
                const int nextRate = std::max(m_targetRate, (rate + 1) / 2);
                if(nextRate == rate) {
                    break;
                }
                plan.push_back(makeStage(rate, nextRate));
                rate = nextRate;
            }
        }

        if(rate != m_targetRate) {
            plan.push_back(makeStage(rate, m_targetRate));
        }
    }
    else {
        plan.push_back(makeStage(m_inputRate, m_targetRate));
    }

    m_stages.clear();
    m_stages.reserve(plan.size());

    for(size_t stageIndex{0}; stageIndex < plan.size(); ++stageIndex) {
        auto& s = plan[stageIndex];
        if(!createStageCtx(m_quality, m_phaseResponse, m_passbandPercent, allowImaging, m_channels, stageIndex, s)) {
            destroyResampler();
            return;
        }
        m_stages.push_back(s);
    }

    refreshLatencyFramesFromStages();
}

void SoxResamplerDSP::refreshLatencyFramesFromStages()
{
    if(m_inputRate <= 0 || m_stages.empty()) {
        m_latencyFrames = 0;
        return;
    }

    // DspNode::latencyFrames is defined in node input-rate frames.
    // Each stage delay is queried in that stage output-rate frames, so
    // convert back into the original input-rate domain before summing.
    double inputDomainLatencyFrames{0.0};

    for(auto& stage : m_stages) {
        if(!stage.ctx || stage.outRate <= 0) {
            stage.latencyFrames = 0;
            continue;
        }

        const double delay  = soxr_delay(stage.ctx);
        stage.latencyFrames = delay > 0.0 ? static_cast<int>(std::ceil(delay)) : 0;
        if(stage.latencyFrames <= 0) {
            continue;
        }

        inputDomainLatencyFrames += (static_cast<double>(stage.latencyFrames) * static_cast<double>(m_inputRate))
                                  / static_cast<double>(stage.outRate);
    }

    m_latencyFrames = static_cast<int>(
        std::clamp(std::ceil(inputDomainLatencyFrames), 0.0, static_cast<double>(std::numeric_limits<int>::max())));
}

int SoxResamplerDSP::currentPendingInputFrames() const
{
    if(m_inputRate <= 0 || m_stages.empty()) {
        return 0;
    }

    double inputDomainLatencyFrames{0.0};

    for(const auto& stage : m_stages) {
        if(!stage.ctx || stage.outRate <= 0) {
            continue;
        }

        const double delay = soxr_delay(stage.ctx);
        if(delay <= 0.0) {
            continue;
        }

        inputDomainLatencyFrames += (delay * static_cast<double>(m_inputRate)) / static_cast<double>(stage.outRate);
    }

    return static_cast<int>(
        std::clamp(std::ceil(inputDomainLatencyFrames), 0.0, static_cast<double>(std::numeric_limits<int>::max())));
}

bool SoxResamplerDSP::ensureResampler()
{
    if(!m_stages.empty()) {
        return true;
    }

    if(!m_format.isValid() || !shouldResampleInputRate(m_format.sampleRate())) {
        return false;
    }

    rebuildResampler();
    return !m_stages.empty();
}

size_t SoxResamplerDSP::estimateOutputFramesForRatio(double ratio, size_t inputFrames)
{
    if(ratio <= 0.0) {
        return inputFrames;
    }

    const auto expected = static_cast<double>(inputFrames) * ratio;
    const auto frames   = static_cast<size_t>(std::ceil(expected));
    return std::max<size_t>(1, frames);
}

bool SoxResamplerDSP::processOneStageBuffer(SoxStage& stage, const ProcessingBuffer& buffer,
                                            ProcessingBufferList& output, const AudioFormat& outFormat)
{
    const auto format = buffer.format();
    if(!format.isValid()) {
        return true;
    }

    const auto inFrames = static_cast<size_t>(buffer.frameCount());
    if(inFrames == 0) {
        return true;
    }

    const auto channels         = static_cast<size_t>(format.channelCount());
    const auto inSamples        = buffer.constData();
    const auto inputSampleCount = static_cast<size_t>(inFrames) * static_cast<size_t>(channels);
    if(inSamples.size() < inputSampleCount) {
        return false;
    }

    const uint64_t inStartNs                  = buffer.startTimeNs();
    const uint64_t inputSourceFrameDurationNs = sourceFrameDurationNsForBuffer(buffer, stage.inRate);
    const uint64_t outputSourceFrameDurationNs
        = mappedSourceFrameDurationNs(inputSourceFrameDurationNs, stage.inRate, stage.outRate);
    stage.outputSourceFrameDurationNs = outputSourceFrameDurationNs;

    if(!stage.hasNextOutTime) {
        stage.hasNextOutTime     = true;
        stage.nextOutTimeNs      = inStartNs;
        stage.nextOutRemainderNs = 0.0;
    }

    size_t consumedFrames{0};
    uint64_t outStartTimeNs    = stage.nextOutTimeNs;
    double outStartRemainderNs = stage.nextOutRemainderNs;
    const auto basePadding     = OutputPaddingFrames + ProcessPaddingFrames;
    auto adaptivePadding       = basePadding;
    size_t iterations{0};

    const auto maxIterations
        = std::max<size_t>(BaseMaxProcessIterations, std::min<size_t>(MaxProcessIterationsCap, inFrames * 4));
    size_t tinyProgressIterations{0};

    while(consumedFrames < inFrames) {
        if(++iterations > maxIterations) {
            const double runtimeDelayFrames = stage.ctx ? soxr_delay(stage.ctx) : 0.0;
            const double runtimeDelayInputFrames
                = (stage.outRate > 0)
                    ? (runtimeDelayFrames * static_cast<double>(stage.inRate)) / static_cast<double>(stage.outRate)
                    : 0.0;

            if(shouldLogWarning(m_warningCounter)) {
                qCWarning(SOXR_RESAMPLER)
                    << "Hit max SoXR iterations without consuming all input:"
                    << "consumedFrames=" << consumedFrames << "inFrames=" << inFrames << "stageIn=" << stage.inRate
                    << "stageOut=" << stage.outRate << "ratio=" << stage.ratio << "passbandEnd=" << stage.passbandEnd
                    << "stopbandBegin=" << stage.stopbandBegin << "runtimeDelayFrames=" << runtimeDelayFrames
                    << "runtimeDelayInputFrames=" << runtimeDelayInputFrames;
            }

            return false;
        }

        const size_t remainingFrames = static_cast<size_t>(inFrames) - consumedFrames;
        const size_t outCapacity
            = std::max<size_t>(1, estimateOutputFramesForRatio(stage.ratio, remainingFrames) + adaptivePadding);
        const size_t outSamples = outCapacity * channels;

        auto* outBuffer = output.addItem(outFormat, outStartTimeNs, outSamples);
        if(!outBuffer) {
            return false;
        }

        auto outSpan = outBuffer->data();
        if(outSpan.size() < outSamples) {
            output.removeByIdx(output.count() - 1);
            return false;
        }

        size_t idone{0};
        size_t odone{0};
        const auto* inData = inSamples.data() + (consumedFrames * channels);

        const soxr_error_t error
            = soxr_process(stage.ctx, inData, remainingFrames, &idone, outSpan.data(), outCapacity, &odone);

        if(error) {
            const double runtimeDelayFrames = stage.ctx ? soxr_delay(stage.ctx) : 0.0;
            const double runtimeDelayInputFrames
                = (stage.outRate > 0)
                    ? (runtimeDelayFrames * static_cast<double>(stage.inRate)) / static_cast<double>(stage.outRate)
                    : 0.0;

            if(shouldLogWarning(m_warningCounter)) {
                qCWarning(SOXR_RESAMPLER)
                    << "soxr_process failed:" << error << "remainingFrames=" << remainingFrames
                    << "outCapacity=" << outCapacity << "stageIn=" << stage.inRate << "stageOut=" << stage.outRate
                    << "passbandEnd=" << stage.passbandEnd << "stopbandBegin=" << stage.stopbandBegin
                    << "runtimeDelayFrames=" << runtimeDelayFrames
                    << "runtimeDelayInputFrames=" << runtimeDelayInputFrames;
            }

            output.removeByIdx(output.count() - 1);
            return false;
        }

        idone = std::min(idone, remainingFrames);

        if(odone == 0) {
            output.removeByIdx(output.count() - 1);
        }
        else {
            outBuffer->resizeSamples(odone * channels);
            outBuffer->setSourceFrameDurationNs(outputSourceFrameDurationNs);
            advanceSourceCursor(outStartTimeNs, outStartRemainderNs, static_cast<int>(odone),
                                outputSourceFrameDurationNs);
        }

        consumedFrames += idone;
        stage.nextOutTimeNs      = outStartTimeNs;
        stage.nextOutRemainderNs = outStartRemainderNs;

        if(idone <= 2 && odone <= 2) {
            ++tinyProgressIterations;

            if(tinyProgressIterations > MaxTinyProgressIterations) {
                const double runtimeDelayFrames = stage.ctx ? soxr_delay(stage.ctx) : 0.0;
                const double runtimeDelayInputFrames
                    = (stage.outRate > 0)
                        ? (runtimeDelayFrames * static_cast<double>(stage.inRate)) / static_cast<double>(stage.outRate)
                        : 0.0;

                if(shouldLogWarning(m_warningCounter)) {
                    qCWarning(SOXR_RESAMPLER)
                        << "SoXR tiny-progress guard triggered"
                        << "idone=" << idone << "odone=" << odone << "remainingFrames=" << remainingFrames
                        << "stageIn=" << stage.inRate << "stageOut=" << stage.outRate
                        << "passbandEnd=" << stage.passbandEnd << "stopbandBegin=" << stage.stopbandBegin
                        << "runtimeDelayFrames=" << runtimeDelayFrames
                        << "runtimeDelayInputFrames=" << runtimeDelayInputFrames;
                }

                return false;
            }
        }
        else {
            tinyProgressIterations = 0;
        }

        if(idone == 0) {
            if(odone == 0) {
                const double runtimeDelayFrames = stage.ctx ? soxr_delay(stage.ctx) : 0.0;
                const double runtimeDelayInputFrames
                    = (stage.outRate > 0)
                        ? (runtimeDelayFrames * static_cast<double>(stage.inRate)) / static_cast<double>(stage.outRate)
                        : 0.0;
                if(shouldLogWarning(m_warningCounter)) {
                    qCWarning(SOXR_RESAMPLER)
                        << "SoXR made no progress (idone=0, odone=0)"
                        << "stageIn=" << stage.inRate << "stageOut=" << stage.outRate
                        << "passbandEnd=" << stage.passbandEnd << "stopbandBegin=" << stage.stopbandBegin
                        << "runtimeDelayFrames=" << runtimeDelayFrames
                        << "runtimeDelayInputFrames=" << runtimeDelayInputFrames;
                }
                return false;
            }
            if(odone == outCapacity && adaptivePadding < static_cast<size_t>(MaxProcessPaddingFrames)) {
                adaptivePadding = std::min(adaptivePadding * 2, static_cast<size_t>(MaxProcessPaddingFrames));
            }
        }
        else {
            adaptivePadding = basePadding;
        }
    }

    return true;
}

void SoxResamplerDSP::processBuffer(const ProcessingBuffer& buffer, ProcessingBufferList& output)
{
    const auto format = buffer.format();
    if(!format.isValid()) {
        return;
    }

    if(format.sampleRate() != m_inputRate || format.channelCount() != m_channels) {
        // Input format changed - rebuild
        prepare(format);

        if(m_stages.empty()) {
            output.addChunk(buffer);
            return;
        }
    }

    const size_t outputCountStart = output.count();

    auto rollbackStageOutput = [&]() {
        while(output.count() > outputCountStart) {
            output.removeByIdx(output.count() - 1);
        }
        if(shouldLogWarning(m_warningCounter)) {
            qCDebug(SOXR_RESAMPLER) << "Dropping chunk and recreating SoXR context";
        }
        destroyResampler();
    };

    m_scratchA.clear();
    m_scratchB.clear();

    // First stage input is the original buffer
    m_scratchA.addChunk(buffer);

    int currentRate{m_inputRate};

    for(size_t stageIndex{0}; stageIndex < m_stages.size(); ++stageIndex) {
        auto& stage = m_stages[stageIndex];

        if(stage.inRate != currentRate) {
            if(shouldLogWarning(m_warningCounter)) {
                qCWarning(SOXR_RESAMPLER) << "Stage chain mismatch:" << "expectedIn=" << currentRate
                                          << "stageIn=" << stage.inRate << "stageOut=" << stage.outRate;
            }

            rollbackStageOutput();
            return;
        }

        AudioFormat stageOutFmt = format;
        stageOutFmt.setSampleRate(stage.outRate);

        ProcessingBufferList& inList  = (stageIndex % 2 == 0) ? m_scratchA : m_scratchB;
        ProcessingBufferList& outList = (stageIndex % 2 == 0) ? m_scratchB : m_scratchA;
        outList.clear();

        const size_t inCount = inList.count();
        for(size_t i{0}; i < inCount; ++i) {
            const auto* inBuf = inList.item(i);
            if(!inBuf || !inBuf->isValid() || inBuf->frameCount() <= 0) {
                continue;
            }
            if(!processOneStageBuffer(stage, *inBuf, outList, stageOutFmt)) {
                rollbackStageOutput();
                return;
            }
        }

        currentRate = stage.outRate;
    }

    // Final output list is m_scratchA if stages count is odd, else m_scratchB
    ProcessingBufferList& finalList = (m_stages.size() % 2 == 0) ? m_scratchA : m_scratchB;
    const size_t outCount           = finalList.count();

    for(size_t i{0}; i < outCount; ++i) {
        const auto* b = finalList.item(i);
        if(b && b->isValid()) {
            output.addChunk(*b);
        }
    }

    m_scratchA.clear();
    m_scratchB.clear();
}
} // namespace Fooyin
