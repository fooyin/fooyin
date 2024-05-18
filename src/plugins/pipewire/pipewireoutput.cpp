/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "pipewireoutput.h"

#include "pipewirecontext.h"
#include "pipewirecore.h"
#include "pipewireregistry.h"
#include "pipewirestream.h"
#include "pipewirethreadloop.h"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>
#include <spa/utils/result.h>

#include <QDebug>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#pragma clang diagnostic ignored "-Wc99-extensions"
#endif

namespace {
spa_audio_format findSpaFormat(const Fooyin::SampleFormat& format)
{
    switch(format) {
        case(Fooyin::SampleFormat::U8):
            return SPA_AUDIO_FORMAT_U8;
        case(Fooyin::SampleFormat::S16):
            return SPA_AUDIO_FORMAT_S16;
        case(Fooyin::SampleFormat::S24):
        case(Fooyin::SampleFormat::S32):
            return SPA_AUDIO_FORMAT_S32;
        case(Fooyin::SampleFormat::Float):
            return SPA_AUDIO_FORMAT_F32;
        case(Fooyin::SampleFormat::Unknown):
        default:
            return SPA_AUDIO_FORMAT_UNKNOWN;
    }
}

void updateChannelMap(spa_audio_info_raw* info, int channels)
{
    // Set channels according to: https://xiph.org/flac/format.html
    switch(channels) {
        case(8):
            info->position[7] = SPA_AUDIO_CHANNEL_SR;
            info->position[6] = SPA_AUDIO_CHANNEL_SL;
            info->position[5] = SPA_AUDIO_CHANNEL_RR;
            info->position[4] = SPA_AUDIO_CHANNEL_RL;
            info->position[3] = SPA_AUDIO_CHANNEL_LFE;
            info->position[2] = SPA_AUDIO_CHANNEL_FC;
            info->position[1] = SPA_AUDIO_CHANNEL_FR;
            info->position[0] = SPA_AUDIO_CHANNEL_FL;
            break;
        case(7):
            info->position[6] = SPA_AUDIO_CHANNEL_SR;
            info->position[5] = SPA_AUDIO_CHANNEL_SL;
            info->position[4] = SPA_AUDIO_CHANNEL_RC;
            info->position[3] = SPA_AUDIO_CHANNEL_LFE;
            info->position[2] = SPA_AUDIO_CHANNEL_FC;
            info->position[1] = SPA_AUDIO_CHANNEL_FR;
            info->position[0] = SPA_AUDIO_CHANNEL_FL;
            break;
        case(6):
            info->position[5] = SPA_AUDIO_CHANNEL_RR;
            info->position[4] = SPA_AUDIO_CHANNEL_RL;
            info->position[3] = SPA_AUDIO_CHANNEL_LFE;
            info->position[2] = SPA_AUDIO_CHANNEL_FC;
            info->position[1] = SPA_AUDIO_CHANNEL_FR;
            info->position[0] = SPA_AUDIO_CHANNEL_FL;
            break;
        case(5):
            info->position[4] = SPA_AUDIO_CHANNEL_RR;
            info->position[3] = SPA_AUDIO_CHANNEL_RL;
            info->position[2] = SPA_AUDIO_CHANNEL_FC;
            info->position[1] = SPA_AUDIO_CHANNEL_FR;
            info->position[0] = SPA_AUDIO_CHANNEL_FL;
            break;
        case(4):
            info->position[3] = SPA_AUDIO_CHANNEL_RR;
            info->position[2] = SPA_AUDIO_CHANNEL_RL;
            info->position[1] = SPA_AUDIO_CHANNEL_FR;
            info->position[0] = SPA_AUDIO_CHANNEL_FL;
            break;
        case(3):
            info->position[2] = SPA_AUDIO_CHANNEL_FC;
            // Fall through
        case(2):
            info->position[1] = SPA_AUDIO_CHANNEL_FR;
            info->position[0] = SPA_AUDIO_CHANNEL_FL;
            break;
        case(1):
            info->position[0] = SPA_AUDIO_CHANNEL_MONO;
            break;
        default:
            break;
    }
}
} // namespace

namespace Fooyin::Pipewire {
struct PipeWireOutput::Private
{
    PipeWireOutput* self;

    QString device;
    float volume{1.0};
    bool pendingVolumeChange{false};

    AudioFormat format;

    AudioBuffer buffer;
    uint32_t bufferPos{0};

    std::unique_ptr<PipewireThreadLoop> loop;
    std::unique_ptr<PipewireContext> context;
    std::unique_ptr<PipewireCore> core;
    std::unique_ptr<PipewireStream> stream;
    std::unique_ptr<PipewireRegistry> registry;

    explicit Private(PipeWireOutput* self_)
        : self{self_}
    { }

    void uninit()
    {
        if(stream) {
            const ThreadLoopGuard guard{loop.get()};
            stream.reset(nullptr);
        }

        if(loop) {
            loop->stop();
        }

        if(core) {
            core.reset(nullptr);
        }

        if(context) {
            context.reset(nullptr);
        }

        if(loop) {
            loop.reset(nullptr);
        }

        if(registry) {
            registry.reset(nullptr);
        }

        buffer.clear();
        bufferPos = 0;
    }

    bool initCore()
    {
        loop = std::make_unique<PipewireThreadLoop>();

        if(!loop->loop()) {
            return false;
        }

        context = std::make_unique<PipewireContext>(loop.get());

        if(!context->context()) {
            return false;
        }

        core = std::make_unique<PipewireCore>(context.get());

        if(!core->core()) {
            return false;
        }

        registry = std::make_unique<PipewireRegistry>(core.get());

        core->syncCore();

        if(!loop->start()) {
            qWarning() << "[PW] Failed to start thread loop";
            return false;
        }

        {
            const ThreadLoopGuard guard{loop.get()};
            while(!core->initialised()) {
                if(loop->wait(2) != 0) {
                    break;
                }
            }
        }

        return core->initialised();
    }

    bool initStream()
    {
        static const pw_stream_events streamEvents = {
            .version       = PW_VERSION_STREAM_EVENTS,
            .state_changed = stateChanged,
            .process       = process,
            .drained       = drained,
        };

        const ThreadLoopGuard guard{loop.get()};

        const auto dev = device != u"default" ? device : QStringLiteral("");

        stream = std::make_unique<PipewireStream>(core.get(), format, dev);
        stream->addListener(streamEvents, this);

        const spa_audio_format spaFormat = findSpaFormat(format.sampleFormat());
        if(spaFormat == SPA_AUDIO_FORMAT_UNKNOWN) {
            qWarning() << "[PW] Unknown audio format";
            return false;
        }

        std::array<uint8_t, 1024> paramBuffer;
        auto builder = SPA_POD_BUILDER_INIT(paramBuffer.data(), paramBuffer.size());

        spa_audio_info_raw audioInfo = {
            .format   = spaFormat,
            .flags    = SPA_AUDIO_FLAG_NONE,
            .rate     = static_cast<uint32_t>(format.sampleRate()),
            .channels = static_cast<uint32_t>(format.channelCount()),
        };

        updateChannelMap(&audioInfo, format.channelCount());

        std::vector<const spa_pod*> params;
        params.emplace_back(spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audioInfo));

        const auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE
                                                        | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS);

        return stream->connect(PW_ID_ANY, PW_DIRECTION_OUTPUT, params, flags);
    }

    static void process(void* userData)
    {
        auto* self = static_cast<PipeWireOutput::Private*>(userData);

        if(!self->bufferPos) {
            self->loop->signal(false);
            return;
        }

        auto* pwBuffer = self->stream->dequeueBuffer();
        if(!pwBuffer) {
            qWarning() << "PW: No available output buffers";
            return;
        }

        const spa_data& data = pwBuffer->buffer->datas[0];

        const auto size = std::min(data.maxsize, self->bufferPos);
        auto* dst       = data.data;

        std::memcpy(dst, self->buffer.data(), self->bufferPos);
        self->bufferPos -= size;
        self->buffer.erase(size);

        data.chunk->offset = 0;
        data.chunk->stride = self->format.bytesPerFrame();
        data.chunk->size   = size;

        self->stream->queueBuffer(pwBuffer);
        self->loop->signal(false);
    }

    static void stateChanged(void* userdata, enum pw_stream_state /*old*/, enum pw_stream_state state,
                             const char* /*error*/)
    {
        auto* self = static_cast<PipeWireOutput::Private*>(userdata);

        if(state == PW_STREAM_STATE_UNCONNECTED) { }
        else if(state == PW_STREAM_STATE_PAUSED || state == PW_STREAM_STATE_STREAMING) {
            self->loop->signal(false);
        }
    }

    static void drained(void* userdata)
    {
        auto* self = static_cast<PipeWireOutput::Private*>(userdata);

        self->stream->setActive(false);
        self->loop->signal(false);
    }
};

PipeWireOutput::PipeWireOutput()
    : p{std::make_unique<Private>(this)}
{ }

PipeWireOutput::~PipeWireOutput() = default;

bool PipeWireOutput::init(const AudioFormat& format)
{
    p->format = format;
    p->buffer = {format, 0};

    pw_init(nullptr, nullptr);

    if(!p->initCore() || !p->initStream()) {
        return false;
    }

    if(p->pendingVolumeChange) {
        p->pendingVolumeChange = false;
        setVolume(p->volume);
    }

    return true;
}

void PipeWireOutput::uninit()
{
    p->uninit();

    pw_deinit();
}

void PipeWireOutput::reset()
{
    {
        const ThreadLoopGuard guard{p->loop.get()};
    }

    p->stream->flush(false);
}

void PipeWireOutput::start()
{
    const ThreadLoopGuard guard{p->loop.get()};
    p->stream->setActive(true);
}

bool PipeWireOutput::initialised() const
{
    return p->core && p->core->initialised();
}

QString PipeWireOutput::device() const
{
    return p->device;
}

bool PipeWireOutput::canHandleVolume() const
{
    return true;
}

OutputDevices PipeWireOutput::getAllDevices() const
{
    OutputDevices devices;

    devices.emplace_back(QStringLiteral("default"), QStringLiteral("Default"));

    if(!p->core || !p->core->initialised()) {
        pw_init(nullptr, nullptr);

        if(!p->initCore()) {
            return {};
        }

        std::ranges::copy(p->registry->devices(), std::back_inserter(devices));

        p->uninit();
    }
    else {
        std::ranges::copy(p->registry->devices(), std::back_inserter(devices));
    }

    return devices;
}

OutputState PipeWireOutput::currentState()
{
    OutputState state;

    state.queuedSamples = p->buffer.frameCount();
    state.freeSamples   = p->stream->bufferSize() - state.queuedSamples;

    return state;
}

int PipeWireOutput::bufferSize() const
{
    return p->stream ? p->stream->bufferSize() : 0;
}

int PipeWireOutput::write(const AudioBuffer& buffer)
{
    const ThreadLoopGuard guard{p->loop.get()};

    p->buffer.append(buffer.constData());
    p->bufferPos += buffer.byteCount();

    return buffer.sampleCount();
}

void PipeWireOutput::setPaused(bool pause)
{
    const ThreadLoopGuard guard{p->loop.get()};
    p->stream->setActive(!pause);
}

void PipeWireOutput::setVolume(double volume)
{
    p->volume = static_cast<float>(volume);

    if(!p->core || !p->core->initialised()) {
        p->pendingVolumeChange = true;
        return;
    }

    const ThreadLoopGuard guard{p->loop.get()};
    p->stream->setVolume(static_cast<float>(volume));
}

void PipeWireOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        p->device = device;
    }
}
} // namespace Fooyin::Pipewire
