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
#include <utils/math.h>
#include <utils/paths.h>

#include <QDebug>

#include <cfenv>
#include <utility>

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

void WaveformGenerator::generate(const Track& track, int samplesPerChannel, bool update)
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
        setState(Idle);
        emit waveformGenerated({});
        return;
    }

    emit generatingWaveform();

    const int bps               = m_format.bytesPerFrame();
    const uint64_t durationSecs = m_data.duration / 1000;
    const int samples           = static_cast<int>(durationSecs * m_format.sampleRate()) / bps * bps;
    const int samplesPerBuffer  = static_cast<int>(static_cast<double>(samples) / samplesPerChannel) / bps * bps;
    const int bufferSize        = samplesPerBuffer * bps;

    const auto endTime = track.offset() + track.duration();

    m_decoder->start();
    m_decoder->seek(track.offset());

    while(true) {
        if(!mayRun()) {
            m_decoder->stop();
            return;
        }

        auto buffer = m_decoder->readBuffer(static_cast<size_t>(bufferSize));
        if(!buffer.isValid() || (track.hasCue() && buffer.endTime() > endTime)) {
            m_data.complete = true;
            break;
        }

        buffer = Audio::convert(buffer, m_requiredFormat);
        processBuffer(buffer);
    }

    m_decoder->stop();

    if(!m_waveDb.storeInCache(trackKey, convertCache<int16_t>(m_data))) {
        qWarning() << "[WaveBar] Unable to store waveform for track:" << m_track.filepath();
    }

    if(!closing()) {
        setState(Idle);
    }

    emit waveformGenerated(m_data);
}

void WaveformGenerator::generateAndRender(const Track& track, int samplesPerChannel, bool update)
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
        WaveformData<int16_t> data;
        if(m_waveDb.loadCachedData(trackKey, data)) {
            const auto floatData = convertCache<float>(data);
            m_data.channelData   = floatData.channelData;
            m_data.complete      = true;

            setState(Idle);
            emit waveformGenerated(m_data);

            return;
        }
    }

    emit generatingWaveform();

    const int bps               = m_format.bytesPerFrame();
    const uint64_t durationSecs = m_data.duration / 1000;
    const int samples           = static_cast<int>(durationSecs * m_format.sampleRate()) / bps * bps;
    const int samplesPerBuffer  = static_cast<int>(static_cast<double>(samples) / samplesPerChannel) / bps * bps;
    const int numOfUpdates      = std::max<int>(1, std::floor(static_cast<double>(durationSecs) / 5));
    const int updateThreshold   = samplesPerChannel / numOfUpdates;

    const int bufferSize = samplesPerBuffer * bps;
    const auto endTime   = track.offset() + track.duration();
    int processedCount{0};

    m_decoder->start();
    m_decoder->seek(track.offset());

    while(true) {
        if(!mayRun()) {
            m_decoder->stop();
            return;
        }

        auto buffer = m_decoder->readBuffer(static_cast<size_t>(bufferSize));
        if(!buffer.isValid() || (track.hasCue() && buffer.endTime() > endTime)) {
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

    if(!closing()) {
        setState(Idle);
    }

    emit waveformGenerated(m_data);
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

    if(!m_decoder || !m_decoder->supportedExtensions().contains(track.extension())) {
        m_decoder = m_audioLoader->decoderForTrack(track);
        if(!m_decoder) {
            return {};
        }
        m_decoder->setMaxLoops(1);
    }

    if(!m_decoder->init(track.filepath())) {
        return {};
    }

    m_track  = track;
    m_format = m_decoder->format();
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
