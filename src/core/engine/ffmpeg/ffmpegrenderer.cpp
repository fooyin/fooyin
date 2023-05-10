/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "ffmpegrenderer.h"

#include "core/engine/audiooutput.h"
#include "ffmpegclock.h"
#include "ffmpegframe.h"
#include "ffmpegutils.h"

extern "C"
{
#include <libswresample/swresample.h>
}

#include <QDebug>

#include <deque>

namespace Fy::Core::Engine::FFmpeg {
constexpr auto MinBufferSize = 10000;

struct SwrContextDeleter
{
    void operator()(SwrContext* context) const
    {
        if(context) {
            swr_free(&context);
        }
    }
};
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

struct Renderer::Private
{
    AudioClock* clock;
    uint64_t position;
    std::deque<Frame> frames;

    AudioOutput* audioOutput;
    OutputContext outputContext;
    SwrContextPtr swrContext;
    QByteArray bufferedData;
    int bufferWritten;

    Private(AudioClock* clock, AudioOutput* output)
        : clock{clock}
        , position{0}
        , audioOutput{output}
        , swrContext{nullptr}
        , bufferWritten{0}
    { }

    std::chrono::microseconds outputRender(const Frame& frame)
    {
        if(!audioOutput) {
            return {};
        }

        if(bufferedData.isNull()) {
            if(!frame.isValid()) {
                return {};
            }

            bufferedData  = interleave(frame.avFrame());
            bufferWritten = 0;
        }

        if(!bufferedData.isNull()) {
            auto bytesWritten
                = audioOutput->write(bufferedData.constData() + bufferWritten, bufferedData.size() - bufferWritten);
            bufferWritten += bytesWritten;

            if(bufferWritten >= bufferedData.size()) {
                bufferedData  = {};
                bufferWritten = 0;
                return {};
            }
            return std::chrono::milliseconds{
                durationForBytes(outputContext,
                                 audioOutput->bufferSize() / outputContext.channelLayout.nb_channels
                                     + bufferedData.size() - bufferWritten)};
        }
        return {};
    }

    QByteArray interleave(const AVFrame* frame)
    {
        const auto format = static_cast<AVSampleFormat>(frame->format);
        if(av_sample_fmt_is_planar(format)) {
            const auto channelLayout = frame->ch_layout;
            const auto channelCount  = channelLayout.nb_channels;
            const auto sampleRate    = frame->sample_rate;
            const auto samplesCount  = frame->nb_samples;

            if(!swrContext) {
                SwrContext* context{nullptr};
                swr_alloc_set_opts2(&context,
                                    &channelLayout,
                                    interleaveFormat(format),
                                    sampleRate,
                                    &channelLayout,
                                    format,
                                    sampleRate,
                                    0,
                                    nullptr);

                int ret = swr_init(context);
                if(ret < 0) {
                    return {};
                }
                swrContext.reset(context);
            }

            const int outSamples = swr_get_out_samples(swrContext.get(), samplesCount);
            std::vector<uint8_t> samples(bytesPerFrame(format, channelCount) * outSamples);
            auto** in = const_cast<const uint8_t**>(frame->extended_data);
            auto* out = static_cast<uint8_t*>(samples.data());
            if(swr_convert(swrContext.get(), &out, outSamples, in, samplesCount) < 0) {
                return {};
            }
            return QByteArray(reinterpret_cast<const char*>(samples.data()), samples.size());
        }
        else {
            return QByteArray::fromRawData(reinterpret_cast<const char*>(frame->data[0]), frame->linesize[0]);
        }
    }
};

Renderer::Renderer(AudioClock* clock, AudioOutput* output, QObject* parent)
    : EngineWorker{parent}
    , p{std::make_unique<Private>(clock, output)}
{
    setObjectName("Renderer");
}

Renderer::~Renderer()
{
    p->frames.clear();
    p->bufferedData = {};
    p->swrContext.reset(nullptr);
    p->bufferWritten = 0;
}

void Renderer::seek(uint64_t pos)
{
    p->position = pos;
    p->frames.clear();
    p->bufferedData  = {};
    p->bufferWritten = 0;
    p->audioOutput->clearBuffer();
}

void Renderer::render(Frame& frame)
{
    if(frame.isValid() && frame.end() < p->position) {
        emit frameProcessed();
        return;
    }

    p->frames.emplace_back(std::move(frame));

    if(p->frames.size() == 1) {
        scheduleNextStep();
    }
}

void Renderer::updateOutput(AVCodecContext* context)
{
    if(!p->audioOutput) {
        return;
    }
    p->outputContext.format        = context->sample_fmt;
    p->outputContext.sampleRate    = context->sample_rate;
    p->outputContext.channelLayout = context->ch_layout;

    p->audioOutput->setBufferSize(bytesForDuration(p->outputContext, MinBufferSize));
    p->audioOutput->init(&p->outputContext);
}

bool Renderer::canDoNextStep() const
{
    return !p->frames.empty() && EngineWorker::canDoNextStep();
}

void Renderer::doNextStep()
{
    const Frame& frame = p->frames.front();

    const bool isValid = frame.isValid();
    const auto result  = p->outputRender(frame);
    const bool done    = result.count() <= 0;

    if(!done && isValid) {
        const auto now = std::chrono::steady_clock::now();
        p->clock->sync(now + result, frame.pts());
    }

    if(done) {
        if(isValid) {
            p->position = std::max(frame.pts(), p->position);
            emit frameProcessed();
        }
        else {
            setAtEnd(true);
        }
        p->frames.pop_front();
    }

    scheduleNextStep(false);
}

int Renderer::timerInterval() const
{
    if(const Frame& frame = p->frames.front(); frame.isValid()) {
        const auto delay = p->clock->timeFromPosition(frame.pts()) - std::chrono::steady_clock::now();
        return std::max(0, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(delay).count()) / 4);
    }
    return 0;
}
} // namespace Fy::Core::Engine::FFmpeg
