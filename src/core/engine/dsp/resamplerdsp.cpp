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

#include "resamplerdsp.h"
#include "resamplersettings.h"

#include <utils/datastream.h>
#include <utils/timeconstants.h>

#include <QDataStream>
#include <QIODevice>

#include <algorithm>
#include <bit>
#include <limits>
#include <optional>
#include <utility>

extern "C"
{
#include <libavcodec/version.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

using namespace Qt::StringLiterals;

#define OLD_CHANNEL_LAYOUT (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100))

constexpr auto MaxDrainIterations  = 64;
constexpr auto OutputFramePadding  = 32;
constexpr auto MaxDrainChunkFrames = 8192;

namespace {
uint64_t channelBit(const Fooyin::AudioFormat::ChannelPosition position)
{
    using P = Fooyin::AudioFormat::ChannelPosition;
    switch(position) {
        case P::FrontLeft:
            return static_cast<uint64_t>(AV_CH_FRONT_LEFT);
        case P::FrontRight:
            return static_cast<uint64_t>(AV_CH_FRONT_RIGHT);
        case P::FrontCenter:
            return static_cast<uint64_t>(AV_CH_FRONT_CENTER);
        case P::LFE:
            return static_cast<uint64_t>(AV_CH_LOW_FREQUENCY);
        case P::BackLeft:
            return static_cast<uint64_t>(AV_CH_BACK_LEFT);
        case P::BackRight:
            return static_cast<uint64_t>(AV_CH_BACK_RIGHT);
        case P::FrontLeftOfCenter:
            return static_cast<uint64_t>(AV_CH_FRONT_LEFT_OF_CENTER);
        case P::FrontRightOfCenter:
            return static_cast<uint64_t>(AV_CH_FRONT_RIGHT_OF_CENTER);
        case P::BackCenter:
            return static_cast<uint64_t>(AV_CH_BACK_CENTER);
        case P::SideLeft:
            return static_cast<uint64_t>(AV_CH_SIDE_LEFT);
        case P::SideRight:
            return static_cast<uint64_t>(AV_CH_SIDE_RIGHT);
        case P::TopCenter:
            return static_cast<uint64_t>(AV_CH_TOP_CENTER);
        case P::TopFrontLeft:
            return static_cast<uint64_t>(AV_CH_TOP_FRONT_LEFT);
        case P::TopFrontCenter:
            return static_cast<uint64_t>(AV_CH_TOP_FRONT_CENTER);
        case P::TopFrontRight:
            return static_cast<uint64_t>(AV_CH_TOP_FRONT_RIGHT);
        case P::TopBackLeft:
            return static_cast<uint64_t>(AV_CH_TOP_BACK_LEFT);
        case P::TopBackCenter:
            return static_cast<uint64_t>(AV_CH_TOP_BACK_CENTER);
        case P::TopBackRight:
            return static_cast<uint64_t>(AV_CH_TOP_BACK_RIGHT);
#ifdef AV_CH_LOW_FREQUENCY_2
        case P::LFE2:
            return static_cast<uint64_t>(AV_CH_LOW_FREQUENCY_2);
#endif
#ifdef AV_CH_TOP_SIDE_LEFT
        case P::TopSideLeft:
            return static_cast<uint64_t>(AV_CH_TOP_SIDE_LEFT);
#endif
#ifdef AV_CH_TOP_SIDE_RIGHT
        case P::TopSideRight:
            return static_cast<uint64_t>(AV_CH_TOP_SIDE_RIGHT);
#endif
#ifdef AV_CH_BOTTOM_FRONT_CENTER
        case P::BottomFrontCenter:
            return static_cast<uint64_t>(AV_CH_BOTTOM_FRONT_CENTER);
#endif
#ifdef AV_CH_BOTTOM_FRONT_LEFT
        case P::BottomFrontLeft:
            return static_cast<uint64_t>(AV_CH_BOTTOM_FRONT_LEFT);
#endif
#ifdef AV_CH_BOTTOM_FRONT_RIGHT
        case P::BottomFrontRight:
            return static_cast<uint64_t>(AV_CH_BOTTOM_FRONT_RIGHT);
#endif
        case P::UnknownPosition:
        default:
            return 0;
    }
}

std::optional<uint64_t> explicitMaskFromLayout(const Fooyin::AudioFormat& format)
{
    if(!format.hasChannelLayout()) {
        return {};
    }

    const int channelCount = std::max(1, format.channelCount());
    const auto layout      = format.channelLayoutView();

    if(std::cmp_less(layout.size(), channelCount)) {
        return {};
    }

    uint64_t mask{0};
    for(int i{0}; i < channelCount; ++i) {
        const uint64_t bit = channelBit(layout[static_cast<size_t>(i)]);

        if(bit == 0 || (mask & bit) != 0) {
            return {};
        }

        mask |= bit;
    }

    if(std::popcount(mask) != channelCount) {
        return {};
    }

    return mask;
}

#if OLD_CHANNEL_LAYOUT
uint64_t channelMaskForFormat(const Fooyin::AudioFormat& format)
{
    if(const auto explicitMask = explicitMaskFromLayout(format)) {
        return *explicitMask;
    }

    const int channelCount   = std::max(1, format.channelCount());
    const int64_t defaultMsk = av_get_default_channel_layout(channelCount);
    if(defaultMsk > 0) {
        return static_cast<uint64_t>(defaultMsk);
    }

    return 0;
}
#else
bool channelLayoutForFormat(const Fooyin::AudioFormat& format, AVChannelLayout& layout)
{
    layout = {};

    const int channelCount = std::max(1, format.channelCount());

    if(const auto explicitMask = explicitMaskFromLayout(format)) {
        if(av_channel_layout_from_mask(&layout, *explicitMask) == 0 && layout.nb_channels == channelCount) {
            return true;
        }
        av_channel_layout_uninit(&layout);
    }

    av_channel_layout_default(&layout, channelCount);
    return layout.nb_channels == channelCount;
}
#endif

std::set<int> parseFilteredSampleRates(const QString& text)
{
    std::set<int> rates;
    QString token;

    const auto parseToken = [&rates](const QString& value) {
        bool ok{false};
        const int rate = value.toInt(&ok);
        if(ok) {
            rates.emplace(rate);
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

Fooyin::ResamplerSettings::SampleRateFilterMode
normaliseSampleRateFilterMode(Fooyin::ResamplerSettings::SampleRateFilterMode mode)
{
    switch(mode) {
        case Fooyin::ResamplerSettings::SampleRateFilterMode::ExcludeListed:
        case Fooyin::ResamplerSettings::SampleRateFilterMode::OnlyListed:
            return mode;
    }

    return Fooyin::ResamplerSettings::SampleRateFilterMode::ExcludeListed;
}

int estimateMaxOutFrames(SwrContext* context, int inFrames, int inputRate, int outputRate)
{
    if(!context) {
        return 0;
    }

    const int safeInFrames   = std::max(0, inFrames);
    const int safeInputRate  = std::max(1, inputRate);
    const int safeOutputRate = std::max(1, outputRate);

    const int suggested = swr_get_out_samples(context, safeInFrames);
    if(suggested > 0) {
        return suggested;
    }

    const int64_t delay = std::max<int64_t>(0, swr_get_delay(context, safeInputRate));
    const int64_t scaled
        = av_rescale_rnd(delay + static_cast<int64_t>(safeInFrames), safeOutputRate, safeInputRate, AV_ROUND_UP);
    const int64_t withPadding = scaled + OutputFramePadding;

    return static_cast<int>(std::clamp<int64_t>(withPadding, 1, std::numeric_limits<int>::max()));
}

} // namespace

namespace Fooyin {
ResamplerDsp::ResamplerDsp()
    : m_swr{nullptr}
    , m_hasOutputCursor{false}
    , m_outputCursorNs{0}
    , m_outputCursorRemainder{0}
    , m_targetRate{ResamplerSettings::DefaultTargetRate}
    , m_useSoxr{ResamplerSettings::DefaultUseSoxr}
    , m_soxrPrecision{ResamplerSettings::DefaultSoxrPrecision}
    , m_sampleRateFilterMode{ResamplerSettings::SampleRateFilterMode::ExcludeListed}

{ }

ResamplerDsp::~ResamplerDsp()
{
    destroyContext();
}

QString ResamplerDsp::name() const
{
    QString baseName = u"Resampler (FFmpeg)"_s;
    if(m_targetRate <= 0) {
        return baseName;
    }

    return u"%1: %2 Hz"_s.arg(baseName).arg(m_targetRate);
}

QString ResamplerDsp::id() const
{
    return u"fooyin.dsp.resampler"_s;
}

AudioFormat ResamplerDsp::outputFormat(const AudioFormat& input) const
{
    if(!input.isValid()) {
        return {};
    }

    if(!shouldResampleInputRate(input.sampleRate())) {
        return input;
    }

    AudioFormat out{input};
    out.setSampleFormat(SampleFormat::F64);
    out.setSampleRate(m_targetRate);

    return out;
}

void ResamplerDsp::prepare(const AudioFormat& format)
{
    m_inputFormat = format;
    m_inputFormat.setSampleFormat(SampleFormat::F64);
    m_outputFormat = outputFormat(m_inputFormat);

    m_hasOutputCursor       = false;
    m_outputCursorNs        = 0;
    m_outputCursorRemainder = 0;

    recreateContext();
}

void ResamplerDsp::process(ProcessingBufferList& chunks)
{
    const size_t count = chunks.count();
    if(count == 0 || !m_inputFormat.isValid()) {
        return;
    }

    ProcessingBufferList output;
    for(size_t i = 0; i < count; ++i) {
        const auto* buffer = chunks.item(i);
        if(!buffer || !buffer->isValid() || buffer->frameCount() <= 0) {
            continue;
        }

        processBuffer(*buffer, output);
    }

    chunks.clear();
    const size_t outCount = output.count();
    for(size_t i = 0; i < outCount; ++i) {
        const auto* buffer = output.item(i);
        if(buffer && buffer->isValid()) {
            chunks.addChunk(*buffer);
        }
    }
}

void ResamplerDsp::reset()
{
    destroyContext();

    m_hasOutputCursor       = false;
    m_outputCursorNs        = 0;
    m_outputCursorRemainder = 0;

    if(m_inputFormat.isValid()) {
        createContext();
    }
}

void ResamplerDsp::flush(ProcessingBufferList& chunks, FlushMode mode)
{
    if(mode == FlushMode::Flush) {
        reset();
        return;
    }

    if(!m_inputFormat.isValid() || !shouldResampleInputRate(m_inputFormat.sampleRate())) {
        reset();
        return;
    }

    if(!m_swr && !createContext()) {
        reset();
        return;
    }

    if(!m_swr) {
        reset();
        return;
    }

    if(!m_hasOutputCursor) {
        m_outputCursorNs        = 0;
        m_outputCursorRemainder = 0;
        m_hasOutputCursor       = true;
    }

    const int outChannels = std::max(1, m_outputFormat.channelCount());
    for(int drainIter = 0; drainIter < MaxDrainIterations; ++drainIter) {
        const int maxOutFrames
            = std::clamp(estimateMaxOutFrames(m_swr, 0, m_inputFormat.sampleRate(), m_outputFormat.sampleRate()), 1,
                         MaxDrainChunkFrames);

        const size_t outSamples = static_cast<size_t>(maxOutFrames) * static_cast<size_t>(outChannels);
        auto* outBuffer         = chunks.addItem(m_outputFormat, m_outputCursorNs, outSamples);
        if(!outBuffer) {
            break;
        }

        auto outData = outBuffer->data();
        if(outData.size() < outSamples) {
            chunks.removeByIdx(chunks.count() - 1);
            break;
        }

        uint8_t* outPlanes[1]{reinterpret_cast<uint8_t*>(outData.data())};
        const int outFrames = swr_convert(m_swr, outPlanes, maxOutFrames, nullptr, 0);
        if(outFrames <= 0) {
            chunks.removeByIdx(chunks.count() - 1);
            break;
        }

        outBuffer->resizeSamples(static_cast<size_t>(outFrames) * static_cast<size_t>(outChannels));
        advanceOutputCursor(outFrames);
    }

    reset();
}

int ResamplerDsp::latencyFrames() const
{
    if(!m_swr || !shouldResampleInputRate(m_inputFormat.sampleRate())) {
        return 0;
    }

    const int inputRate = std::max(1, m_inputFormat.sampleRate());
    const int64_t delay = swr_get_delay(m_swr, inputRate);
    return static_cast<int>(std::clamp<int64_t>(delay, 0, std::numeric_limits<int>::max()));
}

QByteArray ResamplerDsp::saveSettings() const
{
    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<quint32>(ResamplerSettings::SerialisationVersion);
    stream << qint32(m_targetRate);
    stream << m_useSoxr;
    stream << m_soxrPrecision;
    stream << static_cast<qint32>(m_sampleRateFilterMode);
    stream << m_filteredRatesText;

    return data;
}

bool ResamplerDsp::loadSettings(const QByteArray& preset)
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

    qint32 target{ResamplerSettings::DefaultTargetRate};
    bool useSoxrEngine{ResamplerSettings::DefaultUseSoxr};
    double soxrPrecision{ResamplerSettings::DefaultSoxrPrecision};

    auto filterMode           = static_cast<qint32>(ResamplerSettings::DefaultSampleRateFilterMode);
    QString filteredRatesText = QString::fromLatin1(ResamplerSettings::DefaultFilteredRatesText);

    if(version == ResamplerSettings::SerialisationVersion) {
        stream >> target >> useSoxrEngine >> soxrPrecision >> filterMode >> filteredRatesText;
    }
    else {
        return false;
    }

    if(stream.status() != QDataStream::Ok) {
        return false;
    }

    bool recreateRequested{false};

    if(std::exchange(m_targetRate, target) != target) {
        m_outputFormat.setSampleRate(target);
        recreateRequested = true;
    }

    recreateRequested |= (std::exchange(m_useSoxr, useSoxrEngine) != useSoxrEngine);

    if(std::exchange(m_soxrPrecision, soxrPrecision) != soxrPrecision) {
        recreateRequested |= m_useSoxr;
    }

    const auto fMode = normaliseSampleRateFilterMode(static_cast<ResamplerSettings::SampleRateFilterMode>(filterMode));
    recreateRequested |= (std::exchange(m_sampleRateFilterMode, fMode) != fMode);

    const QString trimmedRates = filteredRatesText.trimmed();
    std::set<int> parsedRates  = parseFilteredSampleRates(trimmedRates);
    if(m_filteredRatesText != trimmedRates || m_filteredRates != parsedRates) {
        m_filteredRatesText = trimmedRates;
        m_filteredRates     = std::move(parsedRates);
        recreateRequested   = true;
    }

    if(recreateRequested) {
        recreateContext();
    }

    return true;
}

void ResamplerDsp::setTargetSampleRate(int sampleRate)
{
    if(std::exchange(m_targetRate, sampleRate) != sampleRate) {
        m_outputFormat.setSampleRate(sampleRate);
        recreateContext();
    }
}

int ResamplerDsp::targetSampleRate() const
{
    return m_targetRate;
}

void ResamplerDsp::setUseSoxr(bool useSoxr)
{
    if(std::exchange(m_useSoxr, useSoxr) != useSoxr) {
        recreateContext();
    }
}

bool ResamplerDsp::useSoxr() const
{
    return m_useSoxr;
}

void ResamplerDsp::setSoxrPrecision(double precision)
{
    if(std::exchange(m_soxrPrecision, precision) != precision && m_useSoxr) {
        recreateContext();
    }
}

double ResamplerDsp::soxrPrecision() const
{
    return m_soxrPrecision;
}

void ResamplerDsp::setSampleRateFilterMode(ResamplerSettings::SampleRateFilterMode mode)
{
    const auto normalisedMode = normaliseSampleRateFilterMode(mode);
    if(std::exchange(m_sampleRateFilterMode, normalisedMode) != normalisedMode) {
        recreateContext();
    }
}

ResamplerSettings::SampleRateFilterMode ResamplerDsp::sampleRateFilterMode() const
{
    return m_sampleRateFilterMode;
}

void ResamplerDsp::setFilteredRatesText(const QString& text)
{
    const QString trimmedRates = text.trimmed();
    std::set<int> parsedRates  = parseFilteredSampleRates(trimmedRates);

    if(m_filteredRatesText == trimmedRates && m_filteredRates == parsedRates) {
        return;
    }

    m_filteredRatesText = trimmedRates;
    m_filteredRates     = std::move(parsedRates);
    recreateContext();
}

QString ResamplerDsp::filteredRatesText() const
{
    return m_filteredRatesText;
}

void ResamplerDsp::recreateContext()
{
    destroyContext();
    if(m_inputFormat.isValid()) {
        createContext();
    }
}

bool ResamplerDsp::createContext()
{
    if(!m_inputFormat.isValid()) {
        return false;
    }

    const int inputRate  = std::max(1, m_inputFormat.sampleRate());
    const int outputRate = std::max(1, m_targetRate);

    m_outputFormat = m_inputFormat;
    m_outputFormat.setSampleFormat(SampleFormat::F64);
    m_outputFormat.setSampleRate(outputRate);

    if(!shouldResampleInputRate(inputRate)) {
        return true;
    }

    SwrContext* context = nullptr;

#if OLD_CHANNEL_LAYOUT
    const uint64_t inMask  = channelMaskForFormat(m_inputFormat);
    const uint64_t outMask = channelMaskForFormat(m_outputFormat);

    if(inMask == 0 || outMask == 0) {
        return false;
    }

    context = swr_alloc_set_opts(nullptr, static_cast<int64_t>(outMask), AV_SAMPLE_FMT_DBL, outputRate,
                                 static_cast<int64_t>(inMask), AV_SAMPLE_FMT_DBL, inputRate, 0, nullptr);
    if(!context) {
        return false;
    }
#else
    AVChannelLayout inLayout{};
    AVChannelLayout outLayout{};

    if(!channelLayoutForFormat(m_inputFormat, inLayout) || !channelLayoutForFormat(m_outputFormat, outLayout)) {
        av_channel_layout_uninit(&inLayout);
        av_channel_layout_uninit(&outLayout);
        return false;
    }

    const int rc = swr_alloc_set_opts2(&context, &outLayout, AV_SAMPLE_FMT_DBL, outputRate, &inLayout,
                                       AV_SAMPLE_FMT_DBL, inputRate, 0, nullptr);
    av_channel_layout_uninit(&inLayout);
    av_channel_layout_uninit(&outLayout);
    if(rc < 0 || !context) {
        swr_free(&context);
        return false;
    }
#endif

    if(m_useSoxr) {
        if(av_opt_set(context, "resampler", "soxr", 0) == 0) {
            static_cast<void>(av_opt_set_double(context, "precision", m_soxrPrecision, 0));
        }
    }

    if(swr_init(context) < 0) {
        swr_free(&context);
        return false;
    }

    m_swr = context;
    return true;
}

void ResamplerDsp::destroyContext()
{
    if(m_swr) {
        swr_free(&m_swr);
    }
}

void ResamplerDsp::processBuffer(const ProcessingBuffer& buffer, ProcessingBufferList& output)
{
    AudioFormat format = buffer.format();
    format.setSampleFormat(SampleFormat::F64);
    if(!format.isValid()) {
        return;
    }

    if(format != m_inputFormat) {
        prepare(format);
    }

    if(!shouldResampleInputRate(m_inputFormat.sampleRate())) {
        output.addChunk(buffer);
        return;
    }

    if(!m_swr && !createContext()) {
        return;
    }

    if(!m_swr) {
        return;
    }

    const int inChannels  = std::max(1, m_inputFormat.channelCount());
    const int outChannels = std::max(1, m_outputFormat.channelCount());
    const int inFrames    = buffer.frameCount();
    if(inFrames <= 0) {
        return;
    }

    const auto input             = buffer.constData();
    const size_t expectedSamples = static_cast<size_t>(inFrames) * static_cast<size_t>(inChannels);
    if(input.size() < expectedSamples) {
        return;
    }

    const uint64_t inputStartNs = buffer.startTimeNs();
    if(!m_hasOutputCursor) {
        m_outputCursorNs        = inputStartNs;
        m_outputCursorRemainder = 0;
        m_hasOutputCursor       = true;
    }

    const int maxOutFrames
        = estimateMaxOutFrames(m_swr, inFrames, m_inputFormat.sampleRate(), m_outputFormat.sampleRate());

    const size_t outSamples = static_cast<size_t>(maxOutFrames) * static_cast<size_t>(outChannels);
    auto* outBuffer         = output.addItem(m_outputFormat, m_outputCursorNs, outSamples);
    if(!outBuffer) {
        return;
    }

    auto outData = outBuffer->data();
    if(outData.size() < outSamples) {
        output.removeByIdx(output.count() - 1);
        return;
    }

    const uint8_t* inPlanes[1]{reinterpret_cast<const uint8_t*>(input.data())};
    uint8_t* outPlanes[1]{reinterpret_cast<uint8_t*>(outData.data())};

    const int outFrames = swr_convert(m_swr, outPlanes, maxOutFrames, inPlanes, inFrames);
    if(outFrames <= 0) {
        output.removeByIdx(output.count() - 1);
        return;
    }

    outBuffer->resizeSamples(static_cast<size_t>(outFrames) * static_cast<size_t>(outChannels));
    advanceOutputCursor(outFrames);
}

bool ResamplerDsp::shouldResampleInputRate(const int inputRate) const
{
    if(inputRate <= 0 || inputRate == m_targetRate) {
        return false;
    }

    switch(m_sampleRateFilterMode) {
        case ResamplerSettings::SampleRateFilterMode::ExcludeListed:
            return !m_filteredRates.contains(inputRate);
        case ResamplerSettings::SampleRateFilterMode::OnlyListed:
            return m_filteredRates.contains(inputRate);
    }

    return true;
}

void ResamplerDsp::advanceOutputCursor(const int outFrames)
{
    if(outFrames <= 0) {
        return;
    }

    const uint64_t outputRate       = static_cast<uint64_t>(std::max(1, m_outputFormat.sampleRate()));
    const uint64_t advanceNumerator = (static_cast<uint64_t>(outFrames) * Time::NsPerSecond) + m_outputCursorRemainder;
    const uint64_t advanceNs        = advanceNumerator / outputRate;

    m_outputCursorRemainder = advanceNumerator % outputRate;
    m_outputCursorNs += advanceNs;
}
} // namespace Fooyin
