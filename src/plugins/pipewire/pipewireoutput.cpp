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
#include "pipewireutils.h"

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
        case(Fooyin::SampleFormat::F32):
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
bool PipeWireOutput::init(const AudioFormat& format)
{
    m_format = format;
    m_buffer = {format, 0};

    pw_init(nullptr, nullptr);

    return initCore() && initStream();
}

void PipeWireOutput::uninit()
{
    uninitCore();
    pw_deinit();
}

void PipeWireOutput::reset()
{
    const ThreadLoopGuard guard{m_loop.get()};
    m_stream->flush(false);
}

void PipeWireOutput::start()
{
    const ThreadLoopGuard guard{m_loop.get()};
    m_stream->setActive(true);
    // Setting volume only works consistently when stream is active,
    // so do that here. Fixes issues with silent playback.
    setVolume(m_volume);
}

void PipeWireOutput::drain()
{
    const ThreadLoopGuard guard{m_loop.get()};

    if(m_bufferPos > 0) {
        m_loop->wait(2);
    }
    m_stream->flush(true);
    m_loop->wait(2);
}

bool PipeWireOutput::initialised() const
{
    return m_core && m_core->initialised();
}

QString PipeWireOutput::device() const
{
    return m_device;
}

OutputDevices PipeWireOutput::getAllDevices(bool isCurrentOutput)
{
    OutputDevices devices;

    devices.emplace_back(QStringLiteral("default"), QStringLiteral("Default"));

    if(!isCurrentOutput) {
        pw_init(nullptr, nullptr);
    }

    if(!initCore()) {
        return {};
    }

    std::ranges::copy(m_registry->devices(), std::back_inserter(devices));

    uninitCore();

    if(!isCurrentOutput) {
        pw_deinit();
    }

    return devices;
}

OutputState PipeWireOutput::currentState()
{
    OutputState state;

    state.queuedSamples = m_buffer.frameCount();
    state.freeSamples   = m_stream->bufferSize() - state.queuedSamples;

    return state;
}

int PipeWireOutput::bufferSize() const
{
    return m_stream ? m_stream->bufferSize() : 0;
}

int PipeWireOutput::write(const AudioBuffer& buffer)
{
    const ThreadLoopGuard guard{m_loop.get()};

    m_buffer.append(buffer.constData());
    m_bufferPos += buffer.byteCount();

    return buffer.sampleCount();
}

void PipeWireOutput::setPaused(bool pause)
{
    const ThreadLoopGuard guard{m_loop.get()};
    m_stream->setActive(!pause);
}

void PipeWireOutput::setVolume(double volume)
{
    m_volume = static_cast<float>(volume);

    const ThreadLoopGuard guard{m_loop.get()};
    m_stream->setVolume(static_cast<float>(volume));
}

void PipeWireOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        m_device = device;
    }
}

QString PipeWireOutput::error() const
{
    // TODO
    return {};
}

AudioFormat PipeWireOutput::format() const
{
    return m_format;
}

bool PipeWireOutput::initCore()
{
    m_loop = std::make_unique<PipewireThreadLoop>();

    if(!m_loop->loop()) {
        return false;
    }

    m_context = std::make_unique<PipewireContext>(m_loop.get());

    if(!m_context->context()) {
        return false;
    }

    m_core = std::make_unique<PipewireCore>(m_context.get());

    if(!m_core->core()) {
        return false;
    }

    m_registry = std::make_unique<PipewireRegistry>(m_core.get());

    m_core->syncCore();

    if(!m_loop->start()) {
        qCWarning(PIPEWIRE) << "Failed to start thread loop";
        return false;
    }

    {
        const ThreadLoopGuard guard{m_loop.get()};
        while(!m_core->initialised()) {
            if(m_loop->wait(2) != 0) {
                break;
            }
        }
    }

    return m_core->initialised();
}

bool PipeWireOutput::initStream()
{
    static const pw_stream_events streamEvents = {
        .version       = PW_VERSION_STREAM_EVENTS,
        .state_changed = handleStateChanged,
        .process       = process,
        .drained       = drained,
    };

    const ThreadLoopGuard guard{m_loop.get()};

    const auto dev = m_device != u"default" ? m_device : QString{};

    m_stream = std::make_unique<PipewireStream>(m_core.get(), m_format, dev);
    m_stream->addListener(streamEvents, this);

    const spa_audio_format spaFormat = findSpaFormat(m_format.sampleFormat());
    if(spaFormat == SPA_AUDIO_FORMAT_UNKNOWN) {
        qCWarning(PIPEWIRE) << "Unknown audio format";
        return false;
    }

    std::array<uint8_t, 1024> paramBuffer;
    auto builder = SPA_POD_BUILDER_INIT(paramBuffer.data(), paramBuffer.size());

    spa_audio_info_raw audioInfo = {
        .format   = spaFormat,
        .flags    = SPA_AUDIO_FLAG_NONE,
        .rate     = static_cast<uint32_t>(m_format.sampleRate()),
        .channels = static_cast<uint32_t>(m_format.channelCount()),
    };

    updateChannelMap(&audioInfo, m_format.channelCount());

    std::vector<const spa_pod*> params;
    params.emplace_back(spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audioInfo));

    const auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE
                                                    | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS);

    return m_stream->connect(PW_ID_ANY, PW_DIRECTION_OUTPUT, params, flags);
}

void PipeWireOutput::uninitCore()
{
    if(m_stream) {
        const ThreadLoopGuard guard{m_loop.get()};
        m_stream->flush(false);
        m_stream.reset(nullptr);
    }

    if(m_loop) {
        m_loop->stop();
    }

    if(m_core) {
        m_core.reset(nullptr);
    }

    if(m_context) {
        m_context.reset(nullptr);
    }

    if(m_loop) {
        m_loop.reset(nullptr);
    }

    if(m_registry) {
        m_registry.reset(nullptr);
    }

    m_buffer.clear();
    m_bufferPos = 0;
}

void PipeWireOutput::process(void* userData)
{
    auto* self = static_cast<PipeWireOutput*>(userData);

    if(!self->m_bufferPos) {
        self->m_loop->signal(false);
        return;
    }

    auto* pwBuffer = self->m_stream->dequeueBuffer();
    if(!pwBuffer) {
        qCWarning(PIPEWIRE) << "No available output buffers";
        return;
    }

    const spa_data& data = pwBuffer->buffer->datas[0];

    const auto size = std::min(data.maxsize, self->m_bufferPos);
    auto* dst       = data.data;

    std::memcpy(dst, self->m_buffer.data(), size);
    self->m_bufferPos -= size;
    self->m_buffer.erase(size);

    data.chunk->offset = 0;
    data.chunk->stride = self->m_format.bytesPerFrame();
    data.chunk->size   = size;

    self->m_stream->queueBuffer(pwBuffer);
    self->m_loop->signal(false);
}

void PipeWireOutput::handleStateChanged(void* userdata, pw_stream_state old, pw_stream_state state,
                                        const char* /*error*/)
{
    auto* self = static_cast<PipeWireOutput*>(userdata);

    if(state == PW_STREAM_STATE_UNCONNECTED) {
        QMetaObject::invokeMethod(self, [self]() { emit self->stateChanged(PipeWireOutput::State::Disconnected); });
    }
    else if(old == PW_STREAM_STATE_UNCONNECTED && state == PW_STREAM_STATE_CONNECTING) {
        // TODO: Handle reconnections
    }
    else if(state == PW_STREAM_STATE_PAUSED || state == PW_STREAM_STATE_STREAMING) {
        self->m_loop->signal(false);
    }
}

void PipeWireOutput::drained(void* userdata)
{
    auto* self = static_cast<PipeWireOutput*>(userdata);
    self->m_loop->signal(false);
}
} // namespace Fooyin::Pipewire
