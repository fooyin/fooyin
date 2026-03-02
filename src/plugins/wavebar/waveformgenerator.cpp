/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "waveformgenerator.h"

#include <core/engine/audioconverter.h>
#include <core/engine/audioloader.h>
#include <utils/fymath.h>
#include <utils/fypaths.h>

#include <QDebug>
#include <QFile>

#include <cfenv>
#include <cstring>
#include <limits>
#include <utility>

Q_LOGGING_CATEGORY(WAVEBAR, "fy.wavebar")

namespace {
float convertSampleToFloat(const int16_t inSample)
{
    return static_cast<float>(inSample) / static_cast<float>(std::numeric_limits<int16_t>::max());
}

int16_t convertSampleToInt16(const float inSample)
{
    const int prevRoundingMode = std::fegetround();
    std::fesetround(FE_TONEAREST);

    static constexpr auto minS16 = static_cast<int>(std::numeric_limits<int16_t>::min());
    static constexpr auto maxS16 = static_cast<int>(std::numeric_limits<int16_t>::max());

    int intSample = Fooyin::Math::fltToInt(inSample * 0x8000);
    intSample     = std::clamp(intSample, minS16, maxS16);

    std::fesetround(prevRoundingMode);

    return static_cast<int16_t>(intSample);
}

template <typename OutputType, typename InputType>
Fooyin::WaveBar::WaveformData<OutputType> convertCache(const Fooyin::WaveBar::WaveformData<InputType>& cacheData)
{
    Fooyin::WaveBar::WaveformData<OutputType> data;
    const size_t channels = cacheData.channelData.size();
    data.channelData.resize(channels);

    for(size_t channel{0}; channel < channels; ++channel) {
        auto& inChannelData  = cacheData.channelData[channel];
        auto& outChannelData = data.channelData[channel];

        outChannelData.max.reserve(inChannelData.max.size());
        outChannelData.min.reserve(inChannelData.min.size());
        outChannelData.rms.reserve(inChannelData.rms.size());

        for(const auto& sample : inChannelData.max) {
            if constexpr(std::is_same_v<InputType, int16_t>) {
                outChannelData.max.emplace_back(convertSampleToFloat(sample));
            }
            else {
                outChannelData.max.emplace_back(convertSampleToInt16(sample));
            }
        }
        for(const auto& sample : inChannelData.min) {
            if constexpr(std::is_same_v<InputType, int16_t>) {
                outChannelData.min.emplace_back(convertSampleToFloat(sample));
            }
            else {
                outChannelData.min.emplace_back(convertSampleToInt16(sample));
            }
        }
        for(const auto& sample : inChannelData.rms) {
            if constexpr(std::is_same_v<InputType, int16_t>) {
                outChannelData.rms.emplace_back(convertSampleToFloat(sample));
            }
            else {
                outChannelData.rms.emplace_back(convertSampleToInt16(sample));
            }
        }
    }

    return data;
}
} // namespace

namespace Fooyin::WaveBar {
WaveformGenerator::WaveformGenerator(std::shared_ptr<AudioLoader> audioLoader, DbConnectionPoolPtr dbPool,
                                     QObject* parent)
    : Worker{parent}
    , m_audioLoader{std::move(audioLoader)}
    , m_decoder{nullptr}
    , m_dbPool{std::move(dbPool)}
{
    m_requiredFormat.setSampleFormat(SampleFormat::F32);
}

void WaveformGenerator::initialiseThread()
{
    Worker::initialiseThread();

    m_dbHandler = std::make_unique<DbConnectionHandler>(m_dbPool);
    m_waveDb.initialise(DbConnectionProvider{m_dbPool});
    m_waveDb.initialiseDatabase();
}

void WaveformGenerator::generate(const Track& track, int samplesPerChannel, bool render, bool update)
{
    if(closing()) {
        return;
    }

    const QString trackKey = setup(track, samplesPerChannel);
    if(trackKey.isEmpty()) {
        return;
    }

    setState(Running);

    if(!update && m_waveDb.existsInCache(trackKey)) {
        if(render) {
            WaveformData<int16_t> data;
            if(m_waveDb.loadCachedData(trackKey, data)) {
                const auto floatData = convertCache<float>(data);
                m_data.channelData   = floatData.channelData;
                m_data.complete      = true;

                setState(Idle);
                emit waveformGenerated(track, m_data);
            }
        }
        else {
            setState(Idle);
            emit waveformGenerated(track, {});
        }
        return;
    }

    emit generatingWaveform();

    const int bpf = m_format.bytesPerFrame();
    if(bpf <= 0) {
        qCWarning(WAVEBAR) << "Invalid format while generating waveform for track:" << track.filepath();
        setState(Idle);
        emit waveformGenerated(track, {});
        return;
    }

    const int safeSamplesPerChannel  = std::max(1, samplesPerChannel);
    const uint64_t durationSecs      = m_data.duration / 1000;
    const uint64_t updatesByDuration = std::max<uint64_t>(1, durationSecs / 5);
    const int numOfUpdates           = static_cast<int>(
        std::min<uint64_t>(updatesByDuration, static_cast<uint64_t>(std::numeric_limits<int>::max())));
    const int updateThreshold = std::max(1, safeSamplesPerChannel / numOfUpdates);

    const uint64_t endBytes = m_format.bytesForDuration(track.duration());
    const auto bpfU64       = static_cast<uint64_t>(bpf);

    uint64_t bufferSize
        = endBytes > 0 ? std::max<uint64_t>(bpfU64, endBytes / static_cast<uint64_t>(safeSamplesPerChannel)) : bpfU64;
    bufferSize -= bufferSize % bpfU64;
    bufferSize = std::max<uint64_t>(bufferSize, bpfU64);

    // Keep decoder reads bounded so large-duration/high-rate files do not request
    // oversized chunks in one call.
    static constexpr uint64_t MaxReadBytes = 4ULL * 1024ULL * 1024ULL;
    if(bufferSize > MaxReadBytes) {
        bufferSize = MaxReadBytes - (MaxReadBytes % bpfU64);
        bufferSize = std::max<uint64_t>(bufferSize, bpfU64);
    }

    int processedCount{0};
    uint64_t processedBytes{0};

    m_decoder->start();
    m_decoder->seek(track.offset());

    while(true) {
        if(!mayRun()) {
            m_decoder->stop();
            return;
        }

        if(endBytes > 0 && processedBytes >= endBytes) {
            m_data.complete = true;
            break;
        }

        uint64_t bytesToReadU64 = bufferSize;
        if(endBytes > 0) {
            bytesToReadU64 = std::min<uint64_t>(bufferSize, endBytes - processedBytes);
        }

        const size_t bytesToRead = static_cast<size_t>(
            std::min<uint64_t>(bytesToReadU64, static_cast<uint64_t>(std::numeric_limits<size_t>::max())));

        auto buffer = m_decoder->readBuffer(bytesToRead);
        if(!buffer.isValid()) {
            m_data.complete = true;
            break;
        }

        if(buffer.byteCount() == 0) {
            m_data.complete = true;
            break;
        }

        const uint64_t decodedBytes = static_cast<uint64_t>(buffer.byteCount());
        if(decodedBytes > std::numeric_limits<uint64_t>::max() - processedBytes) {
            processedBytes = std::numeric_limits<uint64_t>::max();
        }
        else {
            processedBytes += decodedBytes;
        }

        buffer = Audio::convert(buffer, m_requiredFormat);
        processBuffer(buffer);

        if(render && processedCount++ == updateThreshold) {
            processedCount = 0;
            emit waveformGenerated(track, m_data);
        }
    }

    m_decoder->stop();

    if(!m_waveDb.storeInCache(trackKey, convertCache<int16_t>(m_data))) {
        qCWarning(WAVEBAR) << "Unable to store waveform for track:" << m_track.filepath();
    }

    if(!closing()) {
        setState(Idle);
    }

    emit waveformGenerated(track, m_data);
}

QString WaveformGenerator::setup(const Track& track, int samplesPerChannel)
{
    if(m_decoder) {
        m_decoder->stop();
    }
    m_data = {};

    if(!track.isValid()) {
        return {};
    }

    if(!track.isInArchive() && !QFile::exists(track.filepath())) {
        return {};
    }

    m_decoder = m_audioLoader->decoderForTrack(track);
    if(!m_decoder) {
        return {};
    }

    AudioSource source;
    source.filepath = track.filepath();
    if(!track.isInArchive()) {
        m_file = std::make_unique<QFile>(track.filepath());
        if(!m_file->open(QIODevice::ReadOnly)) {
            qCWarning(WAVEBAR) << "Failed to open" << track.filepath();
            return {};
        }
        source.device = m_file.get();
    }

    const auto format = m_decoder->init(source, track, AudioDecoder::NoSeeking | AudioDecoder::NoInfiniteLooping);
    if(!format) {
        return {};
    }

    m_track  = track;
    m_format = format.value();
    m_requiredFormat.setChannelCount(m_format.channelCount());
    m_requiredFormat.setSampleRate(m_format.sampleRate());

    m_data.format   = m_requiredFormat;
    m_data.duration = track.duration();
    m_data.channels = m_format.channelCount();
    m_data.channelData.resize(m_data.channels);
    m_data.samplesPerChannel = samplesPerChannel;

    return WaveBarDatabase::cacheKey(m_track, m_data.channels);
}

void WaveformGenerator::processBuffer(const AudioBuffer& buffer)
{
    const int channels   = m_data.channels;
    const int frameCount = buffer.frameCount();
    if(channels <= 0 || frameCount <= 0) {
        return;
    }

    if(buffer.format().bytesPerSample() != static_cast<int>(sizeof(float))) {
        qCWarning(WAVEBAR) << "Unexpected non-float waveform buffer format:" << buffer.format().prettyFormat();
        return;
    }

    const auto sampleCount = static_cast<size_t>(static_cast<uint64_t>(frameCount) * static_cast<uint64_t>(channels));
    std::vector<float> samples(sampleCount);
    std::memcpy(samples.data(), buffer.data(), sampleCount * sizeof(float));

    std::vector<float> channelMax(static_cast<size_t>(channels), -1.0F);
    std::vector<float> channelMin(static_cast<size_t>(channels), 1.0F);
    std::vector<float> channelRms(static_cast<size_t>(channels), 0.0F);

    static constexpr int CancellationCheckInterval = 2048;

    for(int i{0}; i < frameCount; ++i) {
        if((i % CancellationCheckInterval) == 0 && !mayRun()) {
            return;
        }

        const int frameOffset = i * channels;
        for(int ch{0}; ch < channels; ++ch) {
            const float sample                  = samples[static_cast<size_t>(frameOffset + ch)];
            channelMax[static_cast<size_t>(ch)] = std::max(channelMax[static_cast<size_t>(ch)], sample);
            channelMin[static_cast<size_t>(ch)] = std::min(channelMin[static_cast<size_t>(ch)], sample);
            channelRms[static_cast<size_t>(ch)] += sample * sample;
        }
    }

    const float normalise = 1.0F / static_cast<float>(frameCount);
    for(int ch{0}; ch < channels; ++ch) {
        auto& [cMax, cMin, cRms] = m_data.channelData.at(ch);
        cMax.emplace_back(channelMax[static_cast<size_t>(ch)]);
        cMin.emplace_back(channelMin[static_cast<size_t>(ch)]);
        cRms.emplace_back(std::sqrt(channelRms[static_cast<size_t>(ch)] * normalise));
    }
}
} // namespace Fooyin::WaveBar
