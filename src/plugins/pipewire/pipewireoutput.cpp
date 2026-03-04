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

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <limits>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#pragma clang diagnostic ignored "-Wc99-extensions"
#endif

using namespace Qt::StringLiterals;

constexpr auto BufferLength = 200; // ms

namespace {
int clampToInt(const uint64_t value)
{
    static constexpr auto intMax = static_cast<uint64_t>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(value, intMax));
}

int64_t monotonicNsec()
{
    timespec ts{};
    if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }

    return (ts.tv_sec * 1000000000LL) + ts.tv_nsec;
}

int64_t framesToMs(const uint64_t frames, const int sampleRate)
{
    if(sampleRate <= 0) {
        return 0;
    }

    return frames * 1000ULL / static_cast<uint64_t>(sampleRate);
}

spa_audio_format findSpaFormat(const Fooyin::SampleFormat& format)
{
    switch(format) {
        case(Fooyin::SampleFormat::U8):
            return SPA_AUDIO_FORMAT_U8;
        case(Fooyin::SampleFormat::S16):
            return SPA_AUDIO_FORMAT_S16;
        case(Fooyin::SampleFormat::S24In32):
        case(Fooyin::SampleFormat::S32):
            return SPA_AUDIO_FORMAT_S32;
        case(Fooyin::SampleFormat::F32):
            return SPA_AUDIO_FORMAT_F32;
        case(Fooyin::SampleFormat::F64):
            return SPA_AUDIO_FORMAT_F64;
        case(Fooyin::SampleFormat::Unknown):
        default:
            return SPA_AUDIO_FORMAT_UNKNOWN;
    }
}

spa_audio_channel toSpaChannel(Fooyin::AudioFormat::ChannelPosition channel)
{
    using P = Fooyin::AudioFormat::ChannelPosition;
    switch(channel) {
        case P::FrontLeft:
            return SPA_AUDIO_CHANNEL_FL;
        case P::FrontRight:
            return SPA_AUDIO_CHANNEL_FR;
        case P::FrontCenter:
            return SPA_AUDIO_CHANNEL_FC;
        case P::LFE:
            return SPA_AUDIO_CHANNEL_LFE;
        case P::BackLeft:
            return SPA_AUDIO_CHANNEL_RL;
        case P::BackRight:
            return SPA_AUDIO_CHANNEL_RR;
#ifdef SPA_AUDIO_CHANNEL_FLC
        case P::FrontLeftOfCenter:
            return SPA_AUDIO_CHANNEL_FLC;
#endif
#ifdef SPA_AUDIO_CHANNEL_FRC
        case P::FrontRightOfCenter:
            return SPA_AUDIO_CHANNEL_FRC;
#endif
        case P::BackCenter:
            return SPA_AUDIO_CHANNEL_RC;
        case P::SideLeft:
            return SPA_AUDIO_CHANNEL_SL;
        case P::SideRight:
            return SPA_AUDIO_CHANNEL_SR;
#ifdef SPA_AUDIO_CHANNEL_TC
        case P::TopCenter:
            return SPA_AUDIO_CHANNEL_TC;
#endif
#ifdef SPA_AUDIO_CHANNEL_TFL
        case P::TopFrontLeft:
            return SPA_AUDIO_CHANNEL_TFL;
#endif
#ifdef SPA_AUDIO_CHANNEL_TFC
        case P::TopFrontCenter:
            return SPA_AUDIO_CHANNEL_TFC;
#endif
#ifdef SPA_AUDIO_CHANNEL_TFR
        case P::TopFrontRight:
            return SPA_AUDIO_CHANNEL_TFR;
#endif
#ifdef SPA_AUDIO_CHANNEL_TRL
        case P::TopBackLeft:
            return SPA_AUDIO_CHANNEL_TRL;
#endif
#ifdef SPA_AUDIO_CHANNEL_TRC
        case P::TopBackCenter:
            return SPA_AUDIO_CHANNEL_TRC;
#endif
#ifdef SPA_AUDIO_CHANNEL_TRR
        case P::TopBackRight:
            return SPA_AUDIO_CHANNEL_TRR;
#endif
#ifdef SPA_AUDIO_CHANNEL_LFE2
        case P::LFE2:
            return SPA_AUDIO_CHANNEL_LFE2;
#endif
#ifdef SPA_AUDIO_CHANNEL_TSL
        case P::TopSideLeft:
            return SPA_AUDIO_CHANNEL_TSL;
#endif
#ifdef SPA_AUDIO_CHANNEL_TSR
        case P::TopSideRight:
            return SPA_AUDIO_CHANNEL_TSR;
#endif
#ifdef SPA_AUDIO_CHANNEL_BC
        case P::BottomFrontCenter:
            return SPA_AUDIO_CHANNEL_BC;
#endif
#ifdef SPA_AUDIO_CHANNEL_BLC
        case P::BottomFrontLeft:
            return SPA_AUDIO_CHANNEL_BLC;
#endif
#ifdef SPA_AUDIO_CHANNEL_BRC
        case P::BottomFrontRight:
            return SPA_AUDIO_CHANNEL_BRC;
#endif
        case P::UnknownPosition:
        default:
            return SPA_AUDIO_CHANNEL_UNKNOWN;
    }
}

void updateChannelMapLegacy(spa_audio_info_raw* info, int channels)
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

void updateChannelMap(spa_audio_info_raw* info, const Fooyin::AudioFormat& format)
{
    if(format.hasChannelLayout()) {
        bool validMap = true;
        for(int i = 0; i < format.channelCount(); ++i) {
            spa_audio_channel mapped = SPA_AUDIO_CHANNEL_UNKNOWN;
            if(format.channelCount() == 1
               && format.channelPosition(i) == Fooyin::AudioFormat::ChannelPosition::FrontCenter) {
                mapped = SPA_AUDIO_CHANNEL_MONO;
            }
            else {
                mapped = toSpaChannel(format.channelPosition(i));
            }
            info->position[static_cast<size_t>(i)] = mapped;
            if(mapped == SPA_AUDIO_CHANNEL_UNKNOWN) {
                validMap = false;
            }
        }

        if(validMap) {
            return;
        }
    }

    updateChannelMapLegacy(info, format.channelCount());
}

bool supportsPipewireLayout(const Fooyin::AudioFormat& format)
{
    if(!format.hasChannelLayout()) {
        return true;
    }

    for(int i = 0; i < format.channelCount(); ++i) {
        const auto pos = format.channelPosition(i);
        if(format.channelCount() == 1 && pos == Fooyin::AudioFormat::ChannelPosition::FrontCenter) {
            continue;
        }
        if(toSpaChannel(pos) == SPA_AUDIO_CHANNEL_UNKNOWN) {
            return false;
        }
    }

    return true;
}
} // namespace

namespace Fooyin::Pipewire {
bool PipeWireOutput::init(const AudioFormat& format)
{
    m_format             = format;
    m_targetBufferFrames = std::max(1, format.framesForDuration(BufferLength));

    const int bytesPerFrame = format.bytesPerFrame();
    if(bytesPerFrame <= 0) {
        qCWarning(PIPEWIRE) << "Invalid bytes-per-frame for PipeWire output:" << bytesPerFrame;
        return false;
    }

    const auto targetBytes64
        = static_cast<uint64_t>(m_targetBufferFrames) * static_cast<uint64_t>(static_cast<uint32_t>(bytesPerFrame));
    static constexpr auto maxCapacity = std::numeric_limits<size_t>::max() - 1;
    const size_t targetBytes = std::clamp<uint64_t>(targetBytes64, static_cast<uint64_t>(bytesPerFrame), maxCapacity);
    m_buffer                 = std::make_unique<LockFreeRingBuffer<std::byte>>(targetBytes);
    m_lastPwWriteBytes.store(0, std::memory_order_relaxed);

    pw_init(nullptr, nullptr);

    qCDebug(PIPEWIRE) << "PipeWire backend initialised with queue target:" << m_targetBufferFrames << "frames ("
                      << BufferLength << "ms)";

    return initCore() && initStream();
}

void PipeWireOutput::uninit()
{
    uninitCore();
    pw_deinit();
}

void PipeWireOutput::reset()
{
    if(!m_stream || !m_loop) {
        if(m_buffer) {
            m_buffer->requestReset();
        }
        m_lastPwWriteBytes.store(0, std::memory_order_relaxed);
        return;
    }

    const ThreadLoopGuard guard{m_loop.get()};
    m_stream->flush(false);
    if(m_buffer) {
        m_buffer->requestReset();
    }
    m_lastPwWriteBytes.store(0, std::memory_order_relaxed);
}

void PipeWireOutput::start()
{
    if(!m_stream || !m_loop) {
        return;
    }

    const ThreadLoopGuard guard{m_loop.get()};
    m_stream->setActive(true);
    // Setting volume only works consistently when stream is active,
    // so do that here. Fixes issues with silent playback.
    setVolume(m_volume);
}

void PipeWireOutput::drain()
{
    if(!m_stream || !m_loop) {
        return;
    }

    const ThreadLoopGuard guard{m_loop.get()};

    if(m_buffer && m_buffer->readAvailable() > 0) {
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

    devices.emplace_back(u"default"_s, u"Default"_s);

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

    const int bytesPerFrame = m_format.bytesPerFrame();
    const int sampleRate    = m_format.sampleRate();
    if(bytesPerFrame <= 0 || sampleRate <= 0 || !m_buffer) {
        return state;
    }

    const auto softQueuedFrames = [this, bytesPerFrame]() {
        const size_t queuedBytes = m_buffer ? m_buffer->readAvailable() : 0;
        return clampToInt(queuedBytes / static_cast<size_t>(bytesPerFrame));
    };

    const bool hasStreamLoop = (m_stream && m_loop);
    if(!hasStreamLoop) {
        state.queuedFrames = softQueuedFrames();
        state.freeFrames   = std::max(0, bufferSize() - state.queuedFrames);
        state.delay        = static_cast<double>(state.queuedFrames) / static_cast<double>(sampleRate);
        return state;
    }

    const ThreadLoopGuard guard{m_loop.get()};

    const int softwareQueuedFrames = softQueuedFrames();
    state.freeFrames               = std::max(0, bufferSize() - softwareQueuedFrames);
    state.queuedFrames             = softwareQueuedFrames;

    if(const auto time = m_stream->time()) {
        const int64_t softQueuedMs = framesToMs(static_cast<uint64_t>(std::max(0, softwareQueuedFrames)), sampleRate);
        const size_t lastPwWriteBytes = m_lastPwWriteBytes.load(std::memory_order_relaxed);
        const int64_t pwBufferMs      = framesToMs(lastPwWriteBytes / static_cast<size_t>(bytesPerFrame), sampleRate);
        const int64_t nowNsec         = monotonicNsec();

        int64_t timeDiffMs{0};
        if(time->now > 0 && nowNsec > time->now) {
            timeDiffMs = (nowNsec - time->now) / 1000000LL;
        }
        timeDiffMs = std::clamp<int64_t>(timeDiffMs, 0, std::max<int64_t>(0, pwBufferMs));

        int64_t addDelayMs{0};
        addDelayMs += framesToMs(time->queued, sampleRate);
        addDelayMs += framesToMs(time->buffered, sampleRate);

        if(time->rate.denom > 0 && time->rate.num > 0) {
            addDelayMs += static_cast<int64_t>((time->delay * 1000LL * time->rate.num) / time->rate.denom);
        }

        const int64_t totalDelayMs = std::max<int64_t>(0, softQueuedMs + pwBufferMs - timeDiffMs + addDelayMs);
        state.delay                = static_cast<double>(totalDelayMs) / 1000.0;
        state.queuedFrames
            = std::max(state.queuedFrames, clampToInt((static_cast<uint64_t>(totalDelayMs) * sampleRate) / 1000ULL));
    }
    else {
        // Fallback when the stream timing API has no data yet
        state.delay = static_cast<double>(state.queuedFrames) / static_cast<double>(sampleRate);
    }

    return state;
}

int PipeWireOutput::bufferSize() const
{
    return std::max(1, m_targetBufferFrames);
}

int PipeWireOutput::write(const std::span<const std::byte> data, const int frameCount)
{
    if(!m_stream || !m_loop || !m_buffer) {
        return 0;
    }

    if(frameCount <= 0) {
        return 0;
    }

    const int bytesPerFrame = m_format.bytesPerFrame();
    if(bytesPerFrame <= 0) {
        return 0;
    }

    const size_t availableFrames = data.size() / static_cast<size_t>(bytesPerFrame);
    const int framesToWrite      = std::min(frameCount, static_cast<int>(availableFrames));
    if(framesToWrite <= 0) {
        return 0;
    }

    const int freeSoftFrames = clampToInt(m_buffer->writeAvailable() / static_cast<size_t>(bytesPerFrame));
    const int acceptedFrames = std::min(framesToWrite, freeSoftFrames);
    if(acceptedFrames <= 0) {
        return 0;
    }

    const size_t acceptedByteCount = static_cast<size_t>(acceptedFrames) * static_cast<size_t>(bytesPerFrame);
    auto writer                    = m_buffer->writer();
    const size_t writtenBytes      = writer.write(data.data(), acceptedByteCount, RingBufferOverflowPolicy::DropNewest);
    const int writtenFrames        = static_cast<int>(writtenBytes / static_cast<size_t>(bytesPerFrame));

    if(writtenFrames <= 0) {
        return 0;
    }

    return writtenFrames;
}

void PipeWireOutput::setPaused(bool pause)
{
    if(!m_stream || !m_loop) {
        return;
    }

    const ThreadLoopGuard guard{m_loop.get()};
    m_stream->setActive(!pause);
}

void PipeWireOutput::setVolume(double volume)
{
    m_volume = static_cast<float>(volume);

    if(!initialised()) {
        return;
    }

    const ThreadLoopGuard guard{m_loop.get()};
    m_stream->setVolume(static_cast<float>(volume));
}

bool PipeWireOutput::supportsVolumeControl() const
{
    return true;
}

void PipeWireOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        m_device = device;
    }
}

AudioFormat PipeWireOutput::negotiateFormat(const AudioFormat& requested) const
{
    if(!requested.isValid()) {
        return requested;
    }

    if(supportsPipewireLayout(requested)) {
        return requested;
    }

    AudioFormat negotiated = requested;
    const auto fallback    = AudioFormat::defaultChannelLayoutForChannelCount(requested.channelCount());
    if(static_cast<int>(fallback.size()) == requested.channelCount()) {
        negotiated.setChannelLayout(fallback);
    }
    else {
        negotiated.clearChannelLayout();
    }
    return negotiated;
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
    static constexpr pw_stream_events streamEvents = {
        .version       = PW_VERSION_STREAM_EVENTS,
        .state_changed = handleStateChanged,
        .process       = process,
        .drained       = drained,
    };

    const ThreadLoopGuard guard{m_loop.get()};

    const auto dev = m_device != "default"_L1 ? m_device : QString{};

    m_stream = std::make_unique<PipewireStream>(m_core.get(), m_format, m_targetBufferFrames, dev);
    m_stream->addListener(streamEvents, this);

    qCDebug(PIPEWIRE) << "Requesting stream latency:" << m_targetBufferFrames << "/" << m_format.sampleRate()
                      << "frames/rate";

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

    updateChannelMap(&audioInfo, m_format);

    std::vector<const spa_pod*> params;
    params.emplace_back(spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audioInfo));

    static constexpr auto flags = static_cast<pw_stream_flags>(
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS);

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

    m_buffer.reset();
    m_lastPwWriteBytes.store(0, std::memory_order_relaxed);
    m_targetBufferFrames = 0;
}

void PipeWireOutput::process(void* userData)
{
    auto* self = static_cast<PipeWireOutput*>(userData);

    if(!self->m_buffer) {
        self->m_lastPwWriteBytes.store(0, std::memory_order_relaxed);
        self->m_loop->signal(false);
        return;
    }

    const size_t queuedBytes = self->m_buffer->readAvailable();
    if(queuedBytes == 0) {
        self->m_lastPwWriteBytes.store(0, std::memory_order_relaxed);
        self->m_loop->signal(false);
        return;
    }

    const int bytesPerFrame = self->m_format.bytesPerFrame();
    if(bytesPerFrame <= 0) {
        qCWarning(PIPEWIRE) << "Invalid bytes-per-frame in process callback";
        self->m_loop->signal(false);
        return;
    }

    auto* pwBuffer = self->m_stream->dequeueBuffer();
    if(!pwBuffer) {
        qCWarning(PIPEWIRE) << "No available output buffers (queuedBytes=" << queuedBytes << ")";
        return;
    }

    const spa_data& data = pwBuffer->buffer->datas[0];
    if(!data.data || data.maxsize == 0) {
        qCWarning(PIPEWIRE) << "Dequeued buffer has no writable payload";
        self->m_stream->queueBuffer(pwBuffer);
        self->m_loop->signal(false);
        return;
    }

    const size_t maxWritableBytes = std::min(static_cast<size_t>(data.maxsize), queuedBytes);
    const size_t writeBytes
        = (maxWritableBytes / static_cast<size_t>(bytesPerFrame)) * static_cast<size_t>(bytesPerFrame);

    if(writeBytes == 0) {
        data.chunk->offset = 0;
        data.chunk->stride = bytesPerFrame;
        data.chunk->size   = 0;
        self->m_stream->queueBuffer(pwBuffer);
        self->m_lastPwWriteBytes.store(0, std::memory_order_relaxed);
        self->m_loop->signal(false);
        return;
    }

    auto* dst              = static_cast<std::byte*>(data.data);
    auto reader            = self->m_buffer->reader();
    const size_t readBytes = reader.read(dst, writeBytes);

    size_t commitBytes = (readBytes / static_cast<size_t>(bytesPerFrame)) * static_cast<size_t>(bytesPerFrame);
    if(commitBytes == 0) {
        const size_t queuedAfterRead = self->m_buffer->readAvailable();
        if(queuedAfterRead > 0) {
            const size_t retryBytes
                = (std::min(static_cast<size_t>(data.maxsize), queuedAfterRead) / static_cast<size_t>(bytesPerFrame))
                * static_cast<size_t>(bytesPerFrame);

            if(retryBytes > 0) {
                const size_t retryReadBytes = reader.read(dst, retryBytes);
                commitBytes
                    = (retryReadBytes / static_cast<size_t>(bytesPerFrame)) * static_cast<size_t>(bytesPerFrame);
            }
        }

        if(commitBytes == 0) {
            data.chunk->offset = 0;
            data.chunk->stride = bytesPerFrame;
            data.chunk->size   = 0;
            self->m_stream->queueBuffer(pwBuffer);
            self->m_lastPwWriteBytes.store(0, std::memory_order_relaxed);
            self->m_loop->signal(false);
            return;
        }
    }

    data.chunk->offset = 0;
    data.chunk->stride = bytesPerFrame;
    data.chunk->size   = static_cast<uint32_t>(commitBytes);
    self->m_lastPwWriteBytes.store(commitBytes, std::memory_order_relaxed);

    self->m_stream->queueBuffer(pwBuffer);
    self->m_loop->signal(false);
}

void PipeWireOutput::handleStateChanged(void* userdata, pw_stream_state old, pw_stream_state state, const char* error)
{
    auto* self = static_cast<PipeWireOutput*>(userdata);

    if(state == PW_STREAM_STATE_ERROR) {
        const QString message = error ? QString::fromUtf8(error) : u"Unknown PipeWire stream error"_s;
        qCWarning(PIPEWIRE) << "Stream entered error state:" << message;
        QMetaObject::invokeMethod(self, [self]() { emit self->stateChanged(State::Error); });
    }
    else if(state == PW_STREAM_STATE_UNCONNECTED) {
        QMetaObject::invokeMethod(self, [self]() { emit self->stateChanged(State::Disconnected); });
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
