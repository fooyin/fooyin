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

#include "waveformbuilder.h"

#include <core/engine/audioconverter.h>
#include <core/engine/audiodecoder.h>
#include <core/track.h>
#include <utils/crypto.h>
#include <utils/math.h>
#include <utils/paths.h>
#include <utils/settings/settingsmanager.h>

#include <QDebug>
#include <QTimer>

#include <cfenv>

constexpr auto SampleCount     = 2048;
constexpr auto ValuesPerSample = 1;

namespace {
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

Fooyin::DbConnection::DbParams dbConnectionParams()
{
    Fooyin::DbConnection::DbParams params;
    params.type           = QStringLiteral("QSQLITE");
    params.connectOptions = QStringLiteral("QSQLITE_OPEN_URI");
    params.filePath       = Fooyin::Utils::cachePath() + QStringLiteral("wavebar.db");

    return params;
}
} // namespace

namespace Fooyin::WaveBar {
WaveformBuilder::WaveformBuilder(std::unique_ptr<AudioDecoder> decoder, SettingsManager* settings, QObject* parent)
    : Worker{parent}
    , m_decoder{std::move(decoder)}
    , m_settings{settings}
    , m_dbPool{DbConnectionPool::create(dbConnectionParams(), QStringLiteral("wavebar"))}
    , m_downMix{m_settings->value<Settings::WaveBar::Downmix>()}
    , m_channels{0}
    , m_duration{0}
    , m_width{0}
{
    m_requiredFormat.setSampleFormat(SampleFormat::Float);

    m_settings->subscribe<Settings::WaveBar::Downmix>(this, [this](const int downMix) {
        m_downMix = static_cast<DownmixOption>(downMix);
        rescale();
    });
}

void WaveformBuilder::initialiseThread()
{
    m_dbHandler = std::make_unique<DbConnectionHandler>(m_dbPool);
    m_waveDb.initialise(DbConnectionProvider{m_dbPool});
    m_waveDb.initialiseDatabase();
}

void WaveformBuilder::setWidth(const int width)
{
    m_width = width;
}

void WaveformBuilder::rebuild(const Track& track)
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

    m_track    = track;
    m_format   = m_decoder->format();
    m_duration = track.duration();
    m_channels = m_format.channelCount();

    m_requiredFormat.setChannelCount(m_format.channelCount());
    m_requiredFormat.setSampleRate(m_format.sampleRate());

    const QString trackKey = trackCacheKey(m_track, m_channels);

    if(m_waveDb.existsInCache(trackKey)) {
        WaveformData<int16_t> data;
        if(m_waveDb.loadCachedData(trackKey, data)) {
            m_data          = convertCache<float>(data);
            m_data.format   = m_requiredFormat;
            m_data.duration = track.duration();
            emit waveformBuilt();
            return;
        }
    }

    m_data.format   = m_requiredFormat;
    m_data.duration = m_duration;

    m_data.channelData.resize(m_channels);

    const auto samplesPerChannel
        = static_cast<int>(std::floor(static_cast<double>(m_duration) / 1000 * m_format.sampleRate()));
    const auto samplesPerBuffer = static_cast<int>(std::ceil(static_cast<double>(samplesPerChannel) / SampleCount));
    const int bufferSize        = samplesPerBuffer * m_format.bytesPerFrame();

    m_decoder->start();

    while(true) {
        if(!mayRun()) {
            return;
        }

        auto buffer = m_decoder->readBuffer(static_cast<size_t>(bufferSize));
        if(!buffer.isValid()) {
            break;
        }

        buffer = Audio::convert(buffer, m_requiredFormat);
        processBuffer(buffer);
    }

    m_decoder->stop();

    if(!m_waveDb.storeInCache(trackKey, convertCache<int16_t>(m_data))) {
        qWarning() << "[WaveBar] Unable to store waveform for track:" << m_track.filepath();
    }

    setState(Idle);
    emit waveformBuilt();
}

void WaveformBuilder::rescale()
{
    if(m_data.empty()) {
        return;
    }

    setState(Running);

    WaveformData<float> data;
    data.duration = m_data.duration;
    data.format   = m_data.format;

    int channels = m_channels;

    if(m_downMix == DownmixOption::Stereo) {
        channels = 2;
    }
    else if(m_downMix == DownmixOption::Mono) {
        channels = 1;
    }

    data.channels = channels;
    data.channelData.resize(channels);

    constexpr int sampleSize     = ValuesPerSample;
    const double samplesPerPixel = static_cast<double>(SampleCount) / (m_width * (sampleSize));

    for(int ch{0}; ch < channels; ++ch) {
        auto& [outMax, outMin, outRms] = data.channelData.at(ch);

        double start{0.0};

        for(int x{0}; x < m_width; ++x) {
            const double end = std::max(1.0, (x + 1) * samplesPerPixel);

            int sampleCount{0};
            WaveformSample sample;

            if(m_downMix == DownmixOption::Mono) {
                for(int mixCh{0}; mixCh < m_channels; ++mixCh) {
                    sampleCount += buildSample(sample, mixCh, sampleSize, start, end);
                }
            }
            else if(m_downMix == DownmixOption::Stereo && m_channels != 2) {
                for(int mixCh{0}; mixCh < m_channels; ++mixCh) {
                    sampleCount += buildSample(sample, mixCh, sampleSize, start, end);
                }
            }
            else {
                sampleCount += buildSample(sample, ch, sampleSize, start, end);
            }

            sample.rms /= static_cast<float>(sampleCount);
            sample.rms = std::sqrt(sample.rms);

            outMax.emplace_back(sample.max);
            outMin.emplace_back(sample.min);
            outRms.emplace_back(sample.rms);

            start = end;
        }
    }

    setState(Idle);
    emit waveformRescaled(data);
}

void WaveformBuilder::processBuffer(const AudioBuffer& buffer)
{
    const int bps         = buffer.format().bytesPerSample();
    const int sampleCount = buffer.frameCount();
    const auto* samples   = buffer.data();

    for(int ch{0}; ch < m_channels; ++ch) {
        float max{-1.0};
        float min{1.0};
        float rms{0.0};

        for(int i{0}; i < sampleCount; ++i) {
            const int offset = (i * m_channels + ch) * bps;
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

int WaveformBuilder::buildSample(WaveformSample& sample, int channel, int sampleSize, double start, double end)
{
    int sampleCount{0};

    auto& [inMax, inMin, inRms] = m_data.channelData.at(channel);

    const int lastIndex = std::floor(sampleSize * end);

    for(int i = std::floor(start); i < std::ceil(end); ++i) {
        for(int index = i * sampleSize; index < lastIndex; index += sampleSize, ++sampleCount) {
            if(std::cmp_less(index, inMax.size())) {
                const float sampleMax = inMax.at(index);
                const float sampleMin = inMin.at(index);
                const float sampleRms = inRms.at(index);

                sample.max = std::max(sample.max, sampleMax);
                sample.min = std::min(sample.min, sampleMin);
                sample.rms += sampleRms * sampleRms;
            }
        }
    }

    return sampleCount;
}
} // namespace Fooyin::WaveBar
