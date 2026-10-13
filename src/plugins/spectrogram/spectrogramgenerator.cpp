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

#include "spectrogramgenerator.h"

#include "spectrogramconfig.h"

#include <core/engine/audioconverter.h>
#include <core/engine/dsp/realfft.h>

#include <QScopeGuard>

#include <cmath>
#include <limits>
#include <numbers>
#include <stop_token>

using namespace Qt::StringLiterals;

constexpr auto BatchColumns = 16;

namespace Fooyin::Spectrogram {
SpectrogramGenerator::SpectrogramGenerator(std::shared_ptr<AudioLoader> audioLoader)
    : m_audioLoader{std::move(audioLoader)}
    , m_request{0}
{
    qRegisterMetaType<SpectrogramBatch>();
}

uint64_t SpectrogramGenerator::cancel()
{
    const auto request = ++m_request;
    stopThread();
    return request;
}

void SpectrogramGenerator::generate(uint64_t request, const Track& track, int fftSize, int timeColumns,
                                    VisualisationSession::SpectrumWindowFunction windowFunction, bool mono,
                                    int firstChannel, int channelCount)
{
    if(request != m_request.load() || closing()) {
        return;
    }

    setState(Running);

    const auto token      = stopToken();
    const auto resetState = qScopeGuard([this]() { setState(Idle); });
    const auto cancelled  = [this, request, &token]() {
        return request != m_request.load() || token.stop_requested() || closing();
    };

    if(!track.isValid() || track.isRemote() || track.isVirtual() || track.duration() == 0) {
        return;
    }

    auto loaded = m_audioLoader->loadDecoderForTrack(track, AudioDecoder::NoSeeking | AudioDecoder::NoInfiniteLooping);
    if(!loaded.decoder || !loaded.format) {
        Q_EMIT finished(request, u"Unable to open track for analysis."_s);
        return;
    }

    const auto cleanup = qScopeGuard([&loaded]() { loaded.decoder->stop(); });

    if(!loaded.decoder->allowsConcurrentDecoding()) {
        return;
    }

    const auto format{*loaded.format};
    const int inputChannels = format.channelCount();
    const int sampleRate    = format.sampleRate();

    if(inputChannels <= 0 || sampleRate <= 0 || format.bytesPerFrame() <= 0
       || track.duration() > std::numeric_limits<uint64_t>::max() / static_cast<uint64_t>(sampleRate)) {
        Q_EMIT finished(request, u"Invalid audio format or duration."_s);
        return;
    }

    const uint64_t totalFrames = (track.duration() * static_cast<uint64_t>(sampleRate)) / 1000;
    if(totalFrames == 0) {
        return;
    }

    firstChannel = std::clamp(firstChannel, 0, inputChannels - 1);
    channelCount = mono ? 1 : std::clamp(channelCount, 1, std::min(2, inputChannels - firstChannel));

    Dsp::RealFft fft{fftSize};
    if(!fft.isValid()) {
        Q_EMIT finished(request, u"Unable to initialise the FFT."_s);
        return;
    }

    const int bins = fft.binCount();

    std::vector<float> gains(static_cast<size_t>(fftSize));
    float gainSum{0.0F};
    for(int i{0}; i < fftSize; ++i) {
        const float phase = 2.0F * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(fftSize - 1);
        gains[i]          = spectrumWindowGain(windowFunction, phase);
        gainSum += gains[static_cast<size_t>(i)];
    }

    const float magnitudeScale = gainSum > 0.0F ? static_cast<float>(fftSize) / gainSum : 1.0F;

    std::vector ring(static_cast<size_t>(fftSize * channelCount), 0.0F);
    std::vector<float> input(static_cast<size_t>(fftSize));
    std::vector<float> magnitudes(static_cast<size_t>(bins));
    std::vector powers(static_cast<size_t>(bins * channelCount), 0.0);
    int head{0};
    uint64_t frames{0};
    uint64_t transforms{0};

    SpectrogramBatch batch;
    batch.totalColumns
        = static_cast<int>(std::min<uint64_t>(std::clamp(timeColumns, MinTimeColumns, MaxTimeColumns), totalFrames));
    batch.channels = channelCount;

    const auto columnEnd = [totalFrames, columns = static_cast<uint64_t>(batch.totalColumns)](int column) {
        const uint64_t next = column + 1UL;
        return ((totalFrames / columns) * next) + (((totalFrames % columns) * next) / columns);
    };

    const auto analyseWindow = [&]() {
        for(int channel{0}; channel < channelCount; ++channel) {
            for(int i{0}; i < fftSize; ++i) {
                input[i] = ring[(channel * fftSize) + ((head + i) % fftSize)] * gains[i];
            }

            if(!fft.transformMagnitudes(input, magnitudes)) {
                return false;
            }

            for(int bin{0}; bin < bins; ++bin) {
                const double magnitude = static_cast<double>(magnitudes[bin]) * magnitudeScale;
                powers[(channel * bins) + bin] += magnitude * magnitude;
            }
        }
        ++transforms;
        return true;
    };

    int column{0};
    uint64_t intervalEnd = columnEnd(column);
    QString error;

    const auto finishColumn = [&]() {
        SpectrogramColumn spectra;

        for(int channel{0}; channel < channelCount; ++channel) {
            auto& spectrum      = spectra[static_cast<size_t>(channel)];
            spectrum.fftSize    = fftSize;
            spectrum.sampleRate = sampleRate;
            spectrum.magnitudes.resize(static_cast<size_t>(bins));

            for(int bin{0}; bin < bins; ++bin) {
                spectrum.magnitudes[bin]
                    = static_cast<float>(std::sqrt(powers[(channel * bins) + bin] / static_cast<double>(transforms)));
            }
        }

        batch.columns.push_back(std::move(spectra));

        std::ranges::fill(powers, 0.0);
        transforms = 0;
        ++column;
        intervalEnd = columnEnd(column);

        if(batch.columns.size() == BatchColumns || column == batch.totalColumns) {
            Q_EMIT columnsReady(request, batch);
            batch.firstColumn = column;
            batch.columns.clear();
        }
    };

    AudioFormat floatFormat{format};
    floatFormat.setSampleFormat(SampleFormat::F32);
    loaded.decoder->start();

    const std::stop_callback abortDecoder{token, [decoder = loaded.decoder.get()]() { decoder->requestAbort(); }};

    if(cancelled()) {
        return;
    }

    loaded.decoder->seek(track.offset());

    const auto hop = std::max<uint64_t>(1, std::min<uint64_t>(fftSize / 2, totalFrames / batch.totalColumns));

    while(frames < totalFrames && !cancelled()) {
        const auto result = loaded.decoder->readAudio(static_cast<size_t>(format.bytesPerFrame()) * 4096);
        if(result.status == AudioDecoder::ReadStatus::EndOfStream) {
            break;
        }
        if(result.status != AudioDecoder::ReadStatus::DecodedAudio || !result.buffer.isValid()
           || result.buffer.frameCount() == 0 || result.buffer.format() != format) {
            error = result.error.isEmpty() ? u"Unable to decode the track for analysis."_s : result.error;
            break;
        }

        const auto buffer = Audio::convert(result.buffer, floatFormat);
        if(!buffer.isValid()) {
            error = u"Unable to convert the decoded audio."_s;
            break;
        }

        const auto* samples = reinterpret_cast<const float*>(buffer.data());
        for(int frame{0}; frame < buffer.frameCount() && frames < totalFrames; ++frame) {
            if(cancelled()) {
                return;
            }

            for(int channel{0}; channel < channelCount; ++channel) {
                float value{0.0F};
                const int offset = frame * inputChannels;

                if(mono) {
                    for(int ch{0}; ch < inputChannels; ++ch) {
                        value += samples[offset + ch] / static_cast<float>(inputChannels);
                    }
                }
                else {
                    value = samples[offset + firstChannel + channel];
                }

                ring[(channel * fftSize) + head] = value;
            }

            head = (head + 1) % fftSize;
            ++frames;

            if((frames % hop == 0 || (frames == intervalEnd && transforms == 0)) && (!analyseWindow())) {
                Q_EMIT finished(request, u"Unable to calculate the spectrum."_s);
                return;
            }

            if(frames == intervalEnd) {
                finishColumn();
            }
        }
    }

    if(cancelled()) {
        return;
    }

    const uint64_t intervalStart = column > 0 ? columnEnd(column - 1) : 0;
    if(frames > intervalStart && column < batch.totalColumns) {
        if(transforms == 0 && !analyseWindow()) {
            error = u"Unable to calculate the spectrum."_s;
        }
        if(transforms > 0) {
            finishColumn();
        }
    }
    if(!batch.columns.empty()) {
        Q_EMIT columnsReady(request, batch);
    }
    if(error.isEmpty() && column == 0) {
        error = u"No spectral data was decoded."_s;
    }

    Q_EMIT finished(request, error);
}
} // namespace Fooyin::Spectrogram

#include "moc_spectrogramgenerator.cpp"
