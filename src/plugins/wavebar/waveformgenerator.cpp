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
#include <utils/crypto.h>
#include <utils/math.h>
#include <utils/paths.h>

#include <QDebug>

#include <cfenv>

constexpr auto SampleCount = 2048;

namespace {
Fooyin::DbConnection::DbParams dbConnectionParams()
{
    Fooyin::DbConnection::DbParams params;
    params.type           = QStringLiteral("QSQLITE");
    params.connectOptions = QStringLiteral("QSQLITE_OPEN_URI");
    params.filePath       = Fooyin::Utils::cachePath() + QStringLiteral("wavebar.db");

    return params;
}

QString trackCacheKey(const Fooyin::Track& track, int channels)
{
    return Fooyin::Utils::generateHash(track.hash(), QString::number(track.duration()),
                                       QString::number(track.sampleRate()), QString::number(channels));
}

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
WaveformGenerator::WaveformGenerator(std::unique_ptr<AudioDecoder> decoder, QObject* parent)
    : Worker{parent}
    , m_decoder{std::move(decoder)}
    , m_dbPool{DbConnectionPool::create(dbConnectionParams(), QStringLiteral("wavebar"))}
{
    m_requiredFormat.setSampleFormat(SampleFormat::Float);
}

void WaveformGenerator::initialiseThread()
{
    m_dbHandler = std::make_unique<DbConnectionHandler>(m_dbPool);
    m_waveDb.initialise(DbConnectionProvider{m_dbPool});
    m_waveDb.initialiseDatabase();
}

void WaveformGenerator::generate(const Track& track)
{
    setState(Running);

    m_decoder->stop();
    m_data = {};

    if(!track.isValid()) {
        return;
    }

    if(!m_decoder->init(track.filepath())) {
        return;
    }

    m_track  = track;
    m_format = m_decoder->format();
    m_requiredFormat.setChannelCount(m_format.channelCount());
    m_requiredFormat.setSampleRate(m_format.sampleRate());

    m_data.format   = m_requiredFormat;
    m_data.duration = track.duration();
    m_data.channels = m_format.channelCount();
    m_data.channelData.resize(m_data.channels);
    m_data.sampleCount = SampleCount;

    const QString trackKey = trackCacheKey(m_track, m_data.channels);

    if(m_waveDb.existsInCache(trackKey)) {
        WaveformData<int16_t> data;
        if(m_waveDb.loadCachedData(trackKey, data)) {
            const auto floatData = convertCache<float>(data);
            m_data.channelData   = floatData.channelData;
            m_data.complete      = true;
            emit waveformGenerated(m_data);
            return;
        }
    }

    emit generatingWaveform();

    const uint64_t durationSecs = m_data.duration / 1000;
    const int samplesPerChannel = static_cast<int>(std::floor(durationSecs * m_format.sampleRate()));
    const int samplesPerBuffer  = static_cast<int>(std::ceil(static_cast<double>(samplesPerChannel) / SampleCount));
    const int bufferSize        = samplesPerBuffer * m_format.bytesPerFrame();
    const int numOfUpdates      = std::max<int>(1, std::floor(static_cast<double>(durationSecs) / 30));
    const int updateThreshold   = SampleCount / numOfUpdates;

    int processedCount{0};

    m_decoder->start();

    while(true) {
        if(!mayRun()) {
            return;
        }

        auto buffer = m_decoder->readBuffer(static_cast<size_t>(bufferSize));
        if(!buffer.isValid()) {
            m_data.complete = true;
            break;
        }

        buffer = Audio::convert(buffer, m_requiredFormat);
        processBuffer(buffer);

        if(processedCount++ == updateThreshold) {
            processedCount = 0;
            emit waveformGenerated(m_data);
        }
    }

    m_decoder->stop();

    if(!m_waveDb.storeInCache(trackKey, convertCache<int16_t>(m_data))) {
        qWarning() << "[WaveBar] Unable to store waveform for track:" << m_track.filepath();
    }

    setState(Idle);
    emit waveformGenerated(m_data);
}

void WaveformGenerator::processBuffer(const AudioBuffer& buffer)
{
    const int bps         = buffer.format().bytesPerSample();
    const int sampleCount = buffer.frameCount();
    const auto* samples   = buffer.data();

    for(int ch{0}; ch < m_data.channels; ++ch) {
        if(!mayRun()) {
            return;
        }

        float max{-1.0};
        float min{1.0};
        float rms{0.0};

        for(int i{0}; i < sampleCount; ++i) {
            if(!mayRun()) {
                return;
            }

            const int offset = (i * m_data.channels + ch) * bps;
            float sample;
            std::memcpy(&sample, samples + offset, bps);

            max = std::max(max, sample);
            min = std::min(min, sample);
            rms += sample * sample;
        }

        rms /= static_cast<float>(sampleCount);
        rms = std::sqrt(rms);

        auto& [cMax, cMin, cRms] = m_data.channelData.at(ch);
        cMax.emplace_back(max);
        cMin.emplace_back(min);
        cRms.emplace_back(rms);
    }
}
} // namespace Fooyin::WaveBar
