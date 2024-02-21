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

#include <core/constants.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>
#include <spa/utils/result.h>

#include <QDebug>

#include <deque>
#include <mutex>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#pragma clang diagnostic ignored "-Wc99-extensions"
#endif

constexpr auto DefaultDevice     = "default";
constexpr int DefaultBufferCount = 15;

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

struct PwCoreDeleter
{
    void operator()(pw_core* core) const
    {
        if(core) {
            pw_core_disconnect(core);
        }
    }
};
using PwCoreUPtr = std::unique_ptr<pw_core, PwCoreDeleter>;

struct PwPropertiesDeleter
{
    void operator()(pw_properties* properties) const
    {
        if(properties) {
            pw_properties_free(properties);
        }
    }
};
using PwPropertiesUPtr = std::unique_ptr<pw_properties, PwPropertiesDeleter>;

struct PwStreamDeleter
{
    void operator()(pw_stream* stream) const
    {
        if(stream) {
            pw_stream_disconnect(stream);
            pw_stream_destroy(stream);
        }
    }
};
using PwStreamUPtr = std::unique_ptr<pw_stream, PwStreamDeleter>;

struct PwThreadLoopDeleter
{
    void operator()(pw_thread_loop* loop) const
    {
        if(loop) {
            pw_thread_loop_destroy(loop);
        }
    }
};
using PwThreadLoopUPtr = std::unique_ptr<pw_thread_loop, PwThreadLoopDeleter>;

struct PwRegistryDeleter
{
    void operator()(pw_registry* registry) const
    {
        if(registry) {
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry));
        }
    }
};
using PwRegistryUPtr = std::unique_ptr<pw_registry, PwRegistryDeleter>;

struct PwContextDeleter
{
    void operator()(pw_context* context) const
    {
        if(context) {
            pw_context_destroy(context);
        }
    }
};
using PwContextUPtr = std::unique_ptr<pw_context, PwContextDeleter>;

void updateChannelMap(spa_audio_info_raw* info, int channels)
{
    switch(channels) {
        case(9):
            info->position[8] = SPA_AUDIO_CHANNEL_RC;
            // Fall through
        case(8):
            info->position[6] = SPA_AUDIO_CHANNEL_FLC;
            info->position[7] = SPA_AUDIO_CHANNEL_FRC;
            // Fall through
        case(6):
            info->position[4] = SPA_AUDIO_CHANNEL_RL;
            info->position[5] = SPA_AUDIO_CHANNEL_RR;
            // Fall through
        case(4):
            info->position[3] = SPA_AUDIO_CHANNEL_LFE;
            // Fall through
        case(3):
            info->position[2] = SPA_AUDIO_CHANNEL_FC;
            // Fall through
        case(2):
            info->position[0] = SPA_AUDIO_CHANNEL_FL;
            info->position[1] = SPA_AUDIO_CHANNEL_FR;
            break;
        case(1):
            info->position[0] = SPA_AUDIO_CHANNEL_MONO;
            break;
    }
}

struct AudioBufferContext
{
    Fooyin::AudioBuffer buffer;
    int remaining{0};
    std::byte* currentPtr{nullptr};

    explicit AudioBufferContext(Fooyin::AudioBuffer buffer_)
        : buffer{std::move(buffer_)}
        , remaining{buffer.byteCount()}
        , currentPtr{buffer.data()}
    { }

    void advance(int count)
    {
        remaining -= count;
        currentPtr += count;
    }
};

struct PwBufferContext
{
    pw_buffer* buffer{nullptr};
    int remaining{0};
    int total{0};
    std::byte* currentPtr{nullptr};

    void initialise(pw_buffer* buffer_)
    {
        if(buffer_) {
            buffer                = buffer_;
            spa_buffer* spaBuffer = buffer->buffer;
            currentPtr            = static_cast<std::byte*>(spaBuffer->datas[0].data);
            remaining             = spaBuffer->datas[0].maxsize;
            total                 = remaining;
        }
        else {
            reset();
        }
    }

    void reset()
    {
        buffer     = nullptr;
        currentPtr = nullptr;
        remaining  = 0;
        total      = 0;
    }

    void advance(int count)
    {
        remaining -= count;
        currentPtr += count;
    }

    void finalize(pw_stream* stream, int32_t stride)
    {
        if(valid()) {
            const spa_data& data = buffer->buffer->datas[0];
            data.chunk->offset   = 0;
            data.chunk->stride   = stride;
            data.chunk->size     = total - remaining;
            pw_stream_queue_buffer(stream, buffer);
            reset();
        }
    }

    bool valid() const
    {
        return buffer;
    }
};
} // namespace

namespace Fooyin::Pipewire {
struct PipeWireOutput::Private
{
    QString device;
    bool muted{false};
    std::deque<AudioBufferContext> buffers;

    AudioFormat format;

    PwCoreUPtr core;
    PwContextUPtr context;
    PwThreadLoopUPtr loop;
    PwStreamUPtr stream;
    PwRegistryUPtr registry;

    int eventId{0};

    spa_hook coreListener;
    spa_hook streamListener;
    spa_hook registryListener;

    std::mutex bufferMutex;

    bool initialised{false};
    int bufferSize{4096};

    OutputDevices sinks;

    PwBufferContext outBufferContext;

    bool initCore()
    {
        pw_init(nullptr, nullptr);

        loop = PwThreadLoopUPtr{pw_thread_loop_new("fooyin/pipewire", nullptr)};
        if(!loop) {
            qWarning() << "PW: Could not create main loop";
            return false;
        }

        context = PwContextUPtr{pw_context_new(pw_thread_loop_get_loop(loop.get()),
                                               pw_properties_new(PW_KEY_CONFIG_NAME, "client-rt.conf", nullptr), 0)};
        if(!context) {
            qWarning() << "PW: Could not create context";
            return false;
        }

        core = PwCoreUPtr{pw_context_connect(context.get(), nullptr, 0)};
        if(!core) {
            qWarning() << "PW: Could not connect to context";
            return false;
        }

        static constexpr pw_core_events coreEvents = {
            .version = PW_VERSION_CORE_EVENTS,
            .done    = onCoreDone,
            .error   = onCoreError,
        };

        int err = pw_core_add_listener(core.get(), &coreListener, &coreEvents, this); // NOLINT
        if(err < 0) {
            qWarning() << "PW: Could not add core listener";
            return false;
        }

        registry = PwRegistryUPtr{pw_core_get_registry(core.get(), PW_VERSION_REGISTRY, 0)};
        if(!registry) {
            qWarning() << "PW: Could not retrieve registry";
            return false;
        }

        static const pw_registry_events registryEvents = {
            .version = PW_VERSION_REGISTRY_EVENTS,
            .global  = onRegistryEvent,
        };

        err = pw_registry_add_listener(registry.get(), &registryListener, &registryEvents, this); // NOLINT
        if(err < 0) {
            qWarning() << "PW: Could not add registry listener";
            return false;
        }

        eventId = pw_core_sync(core.get(), PW_ID_CORE, 0); // NOLINT

        if(pw_thread_loop_start(loop.get()) != 0) {
            qWarning() << "PW: Could not start thread loop";
            return false;
        }

        pw_thread_loop_lock(loop.get());
        while(!initialised) {
            if(pw_thread_loop_timed_wait(loop.get(), 2) != 0) {
                break;
            }
        }
        pw_thread_loop_unlock(loop.get());

        if(!initialised) {
            qWarning() << "PW: Could not initialise loop";
            return false;
        }

        return true;
    }

    bool initStream()
    {
        pw_thread_loop_lock(loop.get());

        auto properties = PwPropertiesUPtr{pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", nullptr)};
        if(!properties) {
            qWarning() << "PW: Could not create properties";
            pw_thread_loop_unlock(loop.get());
            return false;
        }
        auto* props = properties.get();

        pw_properties_setf(props, PW_KEY_MEDIA_CATEGORY, "Playback");
        pw_properties_setf(props, PW_KEY_MEDIA_ROLE, "Music");
        pw_properties_setf(props, PW_KEY_APP_ID, "fooyin");
        pw_properties_setf(props, PW_KEY_APP_ICON_NAME, "fooyin");
        pw_properties_setf(props, PW_KEY_APP_NAME, "fooyin");
        pw_properties_setf(props, PW_KEY_NODE_ALWAYS_PROCESS, "true");
        pw_properties_setf(props, PW_KEY_NODE_RATE, "1/%u", format.sampleRate());

        if(device != QString::fromLatin1(DefaultDevice)) {
            pw_properties_setf(props, PW_KEY_TARGET_OBJECT, "%s", device.toUtf8().constData());
        }

        // Stream takes ownership of properties
        stream = PwStreamUPtr{pw_stream_new(core.get(), "fooyin", properties.release())};
        if(!stream) {
            qWarning() << "PW: Could not create stream";
            pw_thread_loop_unlock(loop.get());
            return false;
        }

        static constexpr pw_stream_events streamEvents
            = {.version = PW_VERSION_STREAM_EVENTS, .state_changed = onStateChanged, .process = onProcess};
        pw_stream_add_listener(stream.get(), &streamListener, &streamEvents, this);

        const spa_audio_format spaFormat = findSpaFormat(format.sampleFormat());
        if(spaFormat == SPA_AUDIO_FORMAT_UNKNOWN) {
            qWarning() << "PW: Unknown audio format";
            pw_thread_loop_unlock(loop.get());
            return false;
        }

        const spa_pod* params[2];
        uint8_t buffer[4096];
        auto b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

        spa_audio_info_raw audioInfo = {
            .format   = spaFormat,
            .flags    = SPA_AUDIO_FLAG_NONE,
            .rate     = static_cast<uint32_t>(format.sampleRate()),
            .channels = static_cast<uint32_t>(format.channelCount()),
        };

        updateChannelMap(&audioInfo, format.channelCount());

        params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audioInfo);
        params[1] = static_cast<spa_pod*>(
            spa_pod_builder_add_object(&b, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers, SPA_PARAM_BUFFERS_buffers,
                                       SPA_POD_Int(DefaultBufferCount), SPA_PARAM_BUFFERS_size,
                                       SPA_POD_Int(bufferSize * format.bytesPerSample() * format.channelCount()),
                                       SPA_PARAM_BUFFERS_stride, SPA_POD_Int(format.bytesPerFrame())));

        auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE
                                                  | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS);

        if(pw_stream_connect(stream.get(), PW_DIRECTION_OUTPUT, PW_ID_ANY, flags, params, 2) < 0) {
            pw_thread_loop_unlock(loop.get());
            return false;
        }

        pw_thread_loop_unlock(loop.get());
        return true;
    }

    void uninit()
    {
        {
            const std::lock_guard<std::mutex> lock(bufferMutex);
            buffers.clear();
        }

        if(loop) {
            pw_thread_loop_stop(loop.get());

            if(stream) {
                outBufferContext.finalize(stream.get(), format.bytesPerFrame() * format.channelCount());
                stream.reset(nullptr);
            }

            if(registry) {
                registry.reset(nullptr);
            }

            if(context) {
                context.reset(nullptr);
            }

            if(core) {
                core.reset(nullptr);
            }

            loop.reset(nullptr);
        }

        pw_deinit();

        initialised = false;
    }

    static void onCoreError(void* data, uint32_t /*id*/, int /*seq*/, int res, const char* message)
    {
        const auto* self = static_cast<PipeWireOutput::Private*>(data);
        if(!self) {
            return;
        }

        qWarning() << "PW: Error during playback: " << spa_strerror(res) << ", " << message; // NOLINT
        pw_thread_loop_stop(self->loop.get());
    }

    static void onCoreDone(void* data, uint32_t id, int seq)
    {
        auto* self = static_cast<PipeWireOutput::Private*>(data);
        if(!self) {
            return;
        }

        if(id != PW_ID_CORE || seq != self->eventId) {
            return;
        }

        spa_hook_remove(&self->registryListener);
        spa_hook_remove(&self->coreListener);

        self->initialised = true;
        pw_thread_loop_signal(self->loop.get(), false);
    }

    static void onStateChanged(void* data, enum pw_stream_state old, enum pw_stream_state state, const char* /*error*/)
    {
        const auto* self = static_cast<PipeWireOutput::Private*>(data);
        if(!self) {
            return;
        }

        // TODO: Handle errors/disconnections
        if(state == PW_STREAM_STATE_ERROR) { }

        if(state == PW_STREAM_STATE_UNCONNECTED && old != PW_STREAM_STATE_UNCONNECTED) { }
    }

    static void onRegistryEvent(void* data, uint32_t /*id*/, uint32_t /*permissions*/, const char* type,
                                uint32_t /*version*/, const struct spa_dict* props)
    {
        auto* self = static_cast<PipeWireOutput::Private*>(data);
        if(!self) {
            return;
        }

        if(strcmp(type, PW_TYPE_INTERFACE_Node) != 0) {
            return;
        }

        const char* media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        if(!media_class) {
            return;
        }

        if(strcmp(media_class, "Audio/Sink") != 0) {
            return;
        }

        const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
        const char* desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);

        self->sinks.emplace_back(QString::fromLatin1(name), QString::fromLatin1(desc));
    }

    static void onProcess(void* userData)
    {
        auto* self = static_cast<PipeWireOutput::Private*>(userData);

        const std::lock_guard<std::mutex> lock(self->bufferMutex);

        PwBufferContext& outContext = self->outBufferContext;

        if(!outContext.valid()) {
            outContext.initialise(pw_stream_dequeue_buffer(self->stream.get()));
            if(!outContext.valid()) {
                qWarning() << "PW: No available output buffers: ";
                return;
            }
        }

        int stride{0};

        while(outContext.remaining > 0 && !self->buffers.empty()) {
            auto& bufferContext    = self->buffers.front();
            stride                 = bufferContext.buffer.format().bytesPerFrame();
            const int inBufferSize = bufferContext.buffer.byteCount();
            const int bytesToCopy  = std::min(outContext.remaining, inBufferSize);

            std::memcpy(outContext.currentPtr, bufferContext.buffer.constData().data(), bytesToCopy);

            if(outContext.remaining >= bufferContext.remaining) {
                self->buffers.pop_front();
            }
            else {
                bufferContext.advance(bytesToCopy);
            }

            outContext.advance(bytesToCopy);
        }

        if(outContext.remaining == 0) {
            outContext.finalize(self->stream.get(), stride);
        }
    }
};

PipeWireOutput::PipeWireOutput()
    : p{std::make_unique<Private>()}
{ }

PipeWireOutput::~PipeWireOutput() = default;

bool PipeWireOutput::init(const AudioFormat& format)
{
    if(p->initialised) {
        return false;
    }

    p->format = format;

    if(!p->initCore() || !p->initStream()) {
        uninit();
        return false;
    }

    return true;
}

void PipeWireOutput::uninit()
{
    if(p->initialised) {
        p->uninit();
    }
}

void PipeWireOutput::reset()
{
    if(!p->initialised) {
        return;
    }

    pw_thread_loop_lock(p->loop.get());
    {
        const std::lock_guard<std::mutex> lock(p->bufferMutex);
        p->buffers.clear();
    }
    pw_stream_flush(p->stream.get(), false);
    pw_thread_loop_unlock(p->loop.get());
}

void PipeWireOutput::start()
{
    if(!p->initialised) {
        return;
    }

    pw_thread_loop_lock(p->loop.get());
    pw_stream_set_active(p->stream.get(), true);
    pw_thread_loop_unlock(p->loop.get());
}

bool PipeWireOutput::initialised() const
{
    return p->initialised;
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

    devices.emplace_back(QString::fromLatin1(DefaultDevice), QStringLiteral("Default"));

    if(!p->initialised) {
        pw_init(nullptr, nullptr);

        if(!p->initCore()) {
            return {};
        }

        std::ranges::copy(p->sinks, std::back_inserter(devices));

        p->uninit();
    }
    else {
        std::ranges::copy(p->sinks, std::back_inserter(devices));
    }

    return devices;
}

OutputState PipeWireOutput::currentState()
{
    OutputState state;

    const std::lock_guard<std::mutex> lock(p->bufferMutex);

    state.queuedSamples = std::accumulate(p->buffers.cbegin(), p->buffers.cend(), 0,
                                          [](int sum, const auto& buffer) { return sum + buffer.buffer.frameCount(); });

    state.freeSamples = p->bufferSize - state.queuedSamples;

    return state;
}

int PipeWireOutput::bufferSize() const
{
    return p->bufferSize;
}

int PipeWireOutput::write(const AudioBuffer& buffer)
{
    if(!initialised()) {
        return 0;
    }

    const std::lock_guard<std::mutex> lock(p->bufferMutex);

    if(p->buffers.size() >= DefaultBufferCount) {
        return 0;
    }

    p->buffers.emplace_back(buffer);

    return buffer.frameCount();
}

void PipeWireOutput::setPaused(bool pause)
{
    if(p->loop && p->stream) {
        pw_thread_loop_lock(p->loop.get());
        pw_stream_set_active(p->stream.get(), !pause);
        pw_thread_loop_unlock(p->loop.get());
    }
}

void PipeWireOutput::setVolume(double volume)
{
    if(!initialised()) {
        return;
    }

    pw_thread_loop_lock(p->loop.get());

    auto vol = static_cast<float>(volume);

    if(pw_stream_set_control(p->stream.get(), SPA_PROP_volume, 1, &vol, 0) < 0) {
        qDebug() << "PW: Could not set volume";
    }

    pw_thread_loop_unlock(p->loop.get());
}

void PipeWireOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        p->device = device;
    }
}
} // namespace Fooyin::Pipewire
